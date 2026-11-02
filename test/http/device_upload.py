"""Opt-in physical upload/preview/memory regression; requires a designated test device."""
import argparse,base64,http.client,json,re,socket,threading,time,urllib.request
from urllib.parse import urlsplit
from pathlib import Path
parser=argparse.ArgumentParser(description='Opt-in screen upload stress test. Replaces the display; leaves the test animation active. Does not change configuration.')
parser.add_argument('url')
parser.add_argument('--uploads',type=int,default=40)
parser.add_argument('--result',required=True)
args=parser.parse_args()
base=args.url.rstrip('/')
host=urlsplit(base).hostname
root=Path(__file__).resolve().parents[2]
def get(path):
 with urllib.request.urlopen(base+path,timeout=12) as r:return json.load(r)
def screen(body):
 req=urllib.request.Request(base+'/api/screen',json.dumps(body,separators=(',',':')).encode(),{'Content-Type':'application/json'})
 with urllib.request.urlopen(req,timeout=15) as r:assert r.status==200
before=get('/api/matrixinfo')
assert before['clockAutoFallbackActive'] is False
stats={'previews':0,'parseErrors':0,'wsErrors':0,'wrongPixels':0}
expected=set();seen=set();armed=False
stop=threading.Event();ready=threading.Event()
def previews():
 try:
  sock=socket.create_connection((host,81),timeout=8)
  f=sock.makefile('rb')
  key=base64.b64encode(b'pixelit-test-key!').decode()
  sock.sendall(f'GET / HTTP/1.1\r\nHost: {host}:81\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n\r\n'.encode())
  status=f.readline();assert b'101' in status,status
  while True:
   header=f.readline()
   if not header:raise EOFError('Incomplete handshake')
   if header==b'\r\n':break
  ready.set();pending=bytearray()
  def read(n):
   data=f.read(n)
   if len(data)!=n:raise EOFError()
   return data
  while not stop.is_set():
   h=read(2);fin=h[0]&128;op=h[0]&15;n=h[1]&127
   if n==126:n=int.from_bytes(read(2),'big')
   elif n==127:n=int.from_bytes(read(8),'big')
   data=read(n)
   if op==8:break
   if op in (0,1):
    if op==1:pending.clear()
    pending.extend(data)
    if fin:
     try:
      m=json.loads(pending)
      if 'liveview' in m:
       assert len(m['liveview'])==1536
       stats['previews']+=1
       if armed:
        if m['liveview'].upper() not in expected:stats['wrongPixels']+=1
        else:seen.add(m['liveview'].upper())
     except Exception:stats['parseErrors']+=1
  sock.close()
 except Exception as e:
  if not stop.is_set():stats['wsErrors']+=1;print('WS ERROR',repr(e),flush=True)
thread=threading.Thread(target=previews,daemon=True);thread.start()
assert ready.wait(10),'WebSocket handshake failed'
time.sleep(2)
animation=json.loads((root/'examples/animation-32x8.json').read_text())
gamma=next((root/'.pio/libdeps').glob('ESP8266*/Framebuffer GFX/gamma.h')).read_text()
def table(name):return [int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+',re.search(name+r'\[\] = \{(.*?)\}',gamma,re.S)[1])]
g5,g6=table('gamma5'),table('gamma6')
for frame in animation['bitmapAnimation']['data']:
 expected.add(''.join(f'{g5[p>>11]:02X}{g6[(p>>5)&63]:02X}{g5[p&31]:02X}' for p in frame))
screen(animation);time.sleep(2);armed=True
samples=[];maxgap=failed=0;minHeap=minBlock=2**32;errorsChecked=0;rejections=[]
try:
 for i in range(args.uploads):
  screen(animation);time.sleep(1.2)
  s=get('/api/matrixinfo');a=get('/api/animationinfo')
  assert s['uptime']>=before['uptime'],'Restart'
  assert s['animationFrames']==7 and a['animationRenderedFrames']>=7,'Animation interrupted'
  maxgap=max(maxgap,a['animationMaxFrameGap']);failed+=a['animationFailedLedFrames']
  samples.append({k:s[k] for k in ['freeHeap','maxFreeBlock','heapFragmentation']})
  assert a['httpUploadStatus']==200 and a['httpUploadCompactBytes']<6144,a
  minHeap=min(minHeap,a['httpUploadMinHeap']);minBlock=min(minBlock,a['httpUploadMinBlock'])
  if (i+1)%10==0:print({'upload':i+1,**samples[-1],'gap':a['animationMaxFrameGap'],'failed':a['animationFailedLedFrames'],'mqttConnected':s['mqttConnected'],**stats},flush=True)
 # Formatting padding at the existing wire-size boundary remains accepted.
 boundary=json.dumps(animation,separators=(',',':')).encode()
 boundary+=b' '*(12288-len(boundary))
 request=urllib.request.Request(base+'/api/screen',boundary,{'Content-Type':'application/json'})
 with urllib.request.urlopen(request,timeout=15) as r:assert r.status==200
 lastSuccess=get('/api/animationinfo')
 assert lastSuccess['httpUploadInputBytes']==12288
 assert lastSuccess['httpUploadStatus']==200
 # Malformed/oversized requests must deliver JSON errors and preserve playback.
 for payload,status in [(b'{"bitmapAnimation":{"data":[[65536]]}}',(406,)),(b' '*12289,(413,)),(b'{"memoryProbe":"'+b'x'*11000,(406,503))]:
  old=get('/api/animationinfo')
  request=urllib.request.Request(base+'/api/screen',payload,{'Content-Type':'application/json'})
  try:
   urllib.request.urlopen(request,timeout=15)
   raise AssertionError('Rejected request succeeded')
  except urllib.error.HTTPError as e:
   assert e.code in status,(e.code,status)
   actualStatus=e.code
   assert isinstance(json.load(e)['response'],str)
  current=get('/api/animationinfo')
  assert current['animationRenderedFrames']>=old['animationRenderedFrames']
  assert current['httpUploadStatus']==actualStatus
  errorsChecked+=1
  rejections.append({k:current[k] for k in ('httpUploadStatus','httpUploadMinHeap','httpUploadMinBlock')})
 # Fragmented/slow body: the current animation must advance while HTTP receives.
 payload=json.dumps(animation,separators=(',',':')).encode()
 old=get('/api/animationinfo')
 conn=http.client.HTTPConnection(host,timeout=15)
 conn.putrequest('POST','/api/screen');conn.putheader('Content-Type','application/json');conn.putheader('Content-Length',str(len(payload)));conn.endheaders()
 for offset in range(0,len(payload),256):
  conn.send(payload[offset:offset+256]);time.sleep(.05)
 response=conn.getresponse();assert response.status==200,response.read();response.read();conn.close()
 # New animation restarts counters on commit, so check interval (including receive)
 # using a rejected slow request, which cannot reset playback counters.
 old=get('/api/animationinfo')
 conn=http.client.HTTPConnection(host,timeout=15)
 conn.putrequest('POST','/api/screen');conn.putheader('Content-Type','application/json');conn.putheader('Content-Length','12289');conn.endheaders()
 for offset in range(0,12289,256):conn.send(b' '*min(256,12289-offset));time.sleep(.05)
 response=conn.getresponse();assert response.status==413;response.read();conn.close()
 current=get('/api/animationinfo')
 assert current['animationRenderedFrames']>=old['animationRenderedFrames']+10,current
 maxgap=max(maxgap,current['animationMaxFrameGap'])
 time.sleep(2)
 a=get('/api/animationinfo');maxgap=max(maxgap,a['animationMaxFrameGap']);failed+=a['animationFailedLedFrames']
 assert not failed
 assert stats['previews'] and not stats['parseErrors'] and not stats['wsErrors'] and not stats['wrongPixels'],stats
 assert seen==expected,(len(seen),len(expected))
 assert maxgap<=250,maxgap
 result={'uploads':args.uploads,**stats,'pixelExactFrames':len(seen),'errorsChecked':errorsChecked,'lastSuccessfulUpload':lastSuccess,'rejections':rejections,'minInstrumentedHeap':minHeap,'minInstrumentedBlock':minBlock,'maxGap':maxgap,'failed':failed,'minSampledHeap':min(s['freeHeap'] for s in samples),'minSampledBlock':min(s['maxFreeBlock'] for s in samples),'first':samples[0],'last':samples[-1],'final':a}
 Path(args.result).write_text(json.dumps(result,indent=2))
 print('RESULT',json.dumps(result),flush=True)
finally:stop.set()

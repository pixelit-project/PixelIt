"""Opt-in ESP32 limit/HTTP/WebSocket/preview stress test.
Replaces display content, leaves a valid full-matrix animation; no config writes.
Run after installing the ESP32 candidate: device_limits.py URL --result FILE.
"""
import argparse,base64,json,re,socket,struct,threading,time,urllib.request,urllib.error
from pathlib import Path
from urllib.parse import urlsplit

class WebSocket:
 def __init__(self,host):
  self.sock=socket.create_connection((host,81),timeout=12);self.file=self.sock.makefile('rb')
  key=base64.b64encode(b'pixelit-test-key!').decode()
  self.sock.sendall(f'GET / HTTP/1.1\r\nHost: {host}:81\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n\r\n'.encode())
  assert b'101' in self.file.readline()
  while self.file.readline()!=b'\r\n':pass
 def read(self,n):
  b=self.file.read(n)
  if len(b)!=n:raise EOFError()
  return b
 def receive(self):
  pending=bytearray()
  while True:
   h=self.read(2);op=h[0]&15;n=h[1]&127
   if n==126:n=int.from_bytes(self.read(2),'big')
   elif n==127:n=int.from_bytes(self.read(8),'big')
   mask=self.read(4) if h[1]&128 else None
   data=self.read(n)
   if mask:data=bytes(c^mask[i%4] for i,c in enumerate(data))
   if op==8:raise EOFError('WebSocket closed')
   if op in (0,1):
    if op==1:pending.clear()
    pending.extend(data)
    if h[0]&128:return json.loads(pending)
 def send(self,obj):
  data=json.dumps(obj,separators=(',',':')).encode() if not isinstance(obj,bytes) else obj
  n=len(data);header=bytes([129,128|n]) if n<126 else bytes([129,254])+struct.pack('!H',n) if n<65536 else bytes([129,255])+struct.pack('!Q',n)
  mask=b'test';self.sock.sendall(header+mask+bytes(c^mask[i%4] for i,c in enumerate(data)))
 def close(self):
  self.file.close();self.sock.close()

def animation(count,width=32):
 return {'bitmapAnimation':{'size':{'width':width,'height':8},'position':{'x':0,'y':0},'animationDelay':150,'data':[[32768+((i*257+j*13)%32768) for j in range(width*8)] for i in range(count)]}}

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('url');parser.add_argument('--result',required=True);parser.add_argument('--uploads',type=int,default=30);parser.add_argument('--seconds',type=int,default=60);args=parser.parse_args()
 base=args.url.rstrip('/');host=urlsplit(base).hostname
 def get(path):
  with urllib.request.urlopen(base+path,timeout=15) as r:return json.load(r)
 def post(obj,status=200):
  data=json.dumps(obj,separators=(',',':')).encode() if not isinstance(obj,bytes) else obj
  request=urllib.request.Request(base+'/api/screen',data,{'Content-Type':'application/json'})
  try:
   with urllib.request.urlopen(request,timeout=20) as r:code=r.status;reply=json.load(r)
  except urllib.error.HTTPError as e:code=e.code;reply=json.load(e)
  assert code==status,(code,status,reply)
  return len(data)
 time.sleep(2)
 deadline=time.monotonic()+30
 while True:
  try:before=get('/api/matrixinfo');break
  except OSError:
   if time.monotonic()>=deadline:raise
   time.sleep(1)
 assert before['buildSection'].startswith('ESP32');assert not before['clockAutoFallbackActive']
 assert before['maxAnimationFrames']>28 and before['maxAnimationPixels']>1792
 full=before['maxAnimationPixels']//256;small=before['maxAnimationFrames'];wireLimit=before['maxScreenMessageSize']
 result={'before':before,'stages':[]};stop=threading.Event();armed=threading.Event();ready=threading.Event();errors=[];seen=set();previewTimes=[]
 root=Path(__file__).resolve().parents[2]
 gamma=next((root/'.pio/libdeps').glob('ESP32*/Framebuffer GFX/gamma.h')).read_text()
 def table(name):return [int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+',re.search(name+r'\[\] = \{(.*?)\}',gamma,re.S)[1])]
 g5,g6=table('gamma5'),table('gamma6')
 maximum=animation(full)
 expected={''.join(f'{g5[p>>11]:02X}{g6[(p>>5)&63]:02X}{g5[p&31]:02X}' for p in f) for f in maximum['bitmapAnimation']['data']}
 def previews():
  try:
   ws=WebSocket(host);ready.set()
   while not stop.is_set():
    m=ws.receive()
    if 'liveview' in m and armed.is_set():
     frame=m['liveview'].upper();assert frame in expected,'Unexpected preview pixels';seen.add(frame);previewTimes.append(time.monotonic())
  except Exception as e:
   if not stop.is_set():errors.append(repr(e));ready.set()
 thread=threading.Thread(target=previews,daemon=True);thread.start();assert ready.wait(15) and not errors
 def sample(label):
  s=get('/api/matrixinfo');a=get('/api/animationinfo');assert s['uptime']>=before['uptime'],'Restart';assert not errors,(errors,a)
  entry={'label':label,'heap':s['freeHeap'],'block':s['maxFreeBlock'],'lowWaterHeap':s['minFreeHeap'],'frames':s['animationFrames'],**a};result['stages'].append(entry);print(json.dumps(entry),flush=True);return entry
 try:
  for count in sorted(set([7,14,24,32,full])):
   n=post(animation(count));time.sleep(2);s=sample(f'HTTP {count}, {n} bytes');assert s['frames']==count
  # Frame count and pixel count are independent limits, including tiny frames.
  for count,width in [(small,8),(before['maxAnimationPixels']//128,16)]:
   post(animation(count,width));time.sleep(2);assert sample(f'HTTP {count}x{width}x8')['frames']==count
  post(maximum);time.sleep(.4);armed.set()
  previewDeadline=time.monotonic()+full*.15*3+3
  while seen!=expected and time.monotonic()<previewDeadline:
   assert not errors,errors
   time.sleep(.2)
  assert seen==expected,(len(seen),len(expected))
  ws=WebSocket(host)
  def command(obj,wanted='OK'):
   ws.send(obj)
   while True:
    msg=ws.receive()
    if 'response' in msg:
     assert (msg['response']=='OK')==(wanted=='OK'),msg;return msg
  for i in range(args.uploads):
   if i%2:command({'setScreen':maximum})
   else:post(maximum)
   time.sleep(.5)
   s=sample(f'repeated {i+1}');assert s['frames']==full and s['animationMaxFrameGap']<=200,s
  for obj in [animation(full+1),animation(small+1,8)]:
   post(obj,406);command({'setScreen':obj},'error')
  command({'setScreen':{'bitmapAnimation':{'data':[[65536]]}}},'error')
  command(b' '*(wireLimit+1),'error');post(b' '*(wireLimit+1),413)
  # Exact wire boundary; padding does not require extra retained receive storage.
  body=json.dumps(maximum,separators=(',',':')).encode();post(body+b' '*(wireLimit-len(body)))
  command(json.dumps({'setScreen':maximum},separators=(',',':')).encode().ljust(wireLimit,b' '))
  # Rejection leaves counters intact, exposing jitter while slowly draining input.
  old=get('/api/animationinfo');sock=socket.create_connection((host,80),timeout=20)
  sock.sendall(f'POST /api/screen HTTP/1.1\r\nHost: pixelit\r\nContent-Type: application/json\r\nContent-Length: {wireLimit+1}\r\nConnection: close\r\n\r\n'.encode())
  for offset in range(0,wireLimit+1,1024):sock.sendall(b' '*min(1024,wireLimit+1-offset));time.sleep(.04)
  response=sock.recv(4096);assert b'413' in response.split(b'\r\n')[0],response[:100];sock.close()
  s=sample('slow rejected HTTP');assert s['animationRenderedFrames']>old['animationRenderedFrames']+10 and s['animationMaxFrameGap']<=200,s
  post(maximum);start=time.monotonic();last=0
  while time.monotonic()-start<args.seconds:
   time.sleep(5);s=sample('continuous timing');assert s['frames']==full and s['animationRenderedFrames']>last and s['animationMaxFrameGap']<=200,s;last=s['animationRenderedFrames']
  assert last>=args.seconds*1000/150*.9
  result.update({'previews':len(previewTimes),'pixelExactFrames':len(seen),'previewMaxInterval':max((b-a for a,b in zip(previewTimes,previewTimes[1:])),default=0),'errors':errors,'final':get('/api/matrixinfo')})
 finally:
  stop.set();Path(args.result).write_text(json.dumps(result,indent=2))
 print('PASS',args.result,flush=True)
if __name__=='__main__':main()

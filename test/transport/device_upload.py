"""Opt-in physical upload/preview/memory regression; requires a designated test device."""
import argparse,base64,http.client,json,os,queue,re,socket,threading,time,urllib.request
from urllib.parse import urlsplit
from pathlib import Path
parser=argparse.ArgumentParser(description='Opt-in screen upload stress test. Replaces the display; leaves the test animation active. Does not change configuration.')
parser.add_argument('url')
parser.add_argument('--uploads',type=int,default=40)
parser.add_argument('--result',required=True)
parser.add_argument('--mode',choices=['mqtt','websocket'],required=True)
parser.add_argument('--qos',type=int,choices=[0,1],default=1)
args=parser.parse_args()
base=args.url.rstrip('/')
host=urlsplit(base).hostname
root=Path(__file__).resolve().parents[2]
def get(path):
 with urllib.request.urlopen(base+path,timeout=12) as r:return json.load(r)
responses=queue.Queue();links={}
def mqtt_string(value):
 raw=value.encode();return len(raw).to_bytes(2,'big')+raw
def remaining(n):
 out=bytearray()
 while True:
  digit=n%128;n//=128;out.append(digit|(128 if n else 0))
  if not n:return bytes(out)
def mqtt_packet():
 data=links['mqtt'].recv(1)
 if not data:raise EOFError('MQTT closed')
 n=0;factor=1
 while True:
  d=links['mqtt'].recv(1)[0];n+=(d&127)*factor;factor*=128
  if not d&128:break
 payload=bytearray()
 while len(payload)<n:
  chunk=links['mqtt'].recv(n-len(payload))
  if not chunk:raise EOFError('MQTT truncated')
  payload.extend(chunk)
 return data[0],bytes(payload)
def send(payload,expected=(200,),slow=False):
 key='mqttReceive' if args.mode=='mqtt' else 'webSocketReceive'
 previous=get('/api/animationinfo')[key]['messages']
 if args.mode=='mqtt':
  topic=links['topic'];packet_id=links.get('packet',0)+1;links['packet']=packet_id
  body=mqtt_string(topic)+(packet_id.to_bytes(2,'big') if args.qos else b'')+payload
  wire=(b'\x32' if args.qos else b'\x30')+remaining(len(body))+body
  tcp=links['mqtt']
 else:
  mask=os.urandom(4)
  wire=b'\x81'+(bytes([128+len(payload)]) if len(payload)<126 else b'\xfe'+len(payload).to_bytes(2,'big'))+mask+bytes(c^mask[i%4] for i,c in enumerate(payload))
  tcp=links['ws']
 if slow:
  for offset in range(0,len(wire),256):tcp.sendall(wire[offset:offset+256]);time.sleep(.05)
 else:tcp.sendall(wire)
 if args.mode=='mqtt' and args.qos:
  opcode,ack=mqtt_packet();assert opcode==0x40 and ack==packet_id.to_bytes(2,'big'),(opcode,ack)
 elif args.mode=='websocket':
  response=responses.get(timeout=15)
  assert (response=='OK')==(200 in expected),response
 key='mqttReceive' if args.mode=='mqtt' else 'webSocketReceive'
 for attempt in range(20):
  diag=get('/api/animationinfo')[key]
  if diag['messages']>previous and diag['inputBytes']==len(payload) and diag['status'] in expected:return diag
  time.sleep(.05)
 raise AssertionError(diag)
def screen(body):return send(json.dumps(body if args.mode=='mqtt' else {'setScreen':body},separators=(',',':')).encode())
before=get('/api/matrixinfo')
assert before['clockAutoFallbackActive'] is False
stats={'previews':0,'parseErrors':0,'wsErrors':0,'wrongPixels':0}
expected_frames=set();seen=set();armed=False
stop=threading.Event();ready=threading.Event()
def previews():
 try:
  sock=socket.create_connection((host,81),timeout=8)
  links['ws']=sock
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
      if 'response' in m:responses.put(m['response'])
      if 'liveview' in m:
       assert len(m['liveview'])==1536
       stats['previews']+=1
       if armed:
        if m['liveview'].upper() not in expected_frames:stats['wrongPixels']+=1
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
 expected_frames.add(''.join(f'{g5[p>>11]:02X}{g6[(p>>5)&63]:02X}{g5[p&31]:02X}' for p in frame))
if args.mode=='mqtt':
 cfg=get('/api/config');assert cfg['mqttUseDeviceTopic'],'Enable device-specific topics for isolated testing first'
 tcp=socket.create_connection((cfg['mqttServer'],int(cfg['mqttPort'])),timeout=15);links['mqtt']=tcp
 flags=2|(128 if cfg.get('mqttUser') else 0)|(64 if cfg.get('mqttPassword') else 0)
 payload=mqtt_string('PixelIt-test-'+str(time.time_ns()))
 if flags&128:payload+=mqtt_string(cfg['mqttUser'])
 if flags&64:payload+=mqtt_string(cfg['mqttPassword'])
 body=b'\x00\x04MQTT\x04'+bytes([flags])+b'\x00\x3c'+payload
 tcp.sendall(b'\x10'+remaining(len(body))+body)
 assert mqtt_packet()==(0x20,b'\x00\x00'),'MQTT connect rejected'
 links['topic']=cfg['mqttMasterTopic'].rstrip('/')+'/'+cfg['hostname']+'/setScreen'
screen(animation);time.sleep(2);armed=True
samples=[];maximum=0;diags=[]
try:
 for i in range(args.uploads):
  diag=screen(animation);diags.append(diag);time.sleep(1.2)
  state=get('/api/matrixinfo');a=get('/api/animationinfo')
  assert state['uptime']>=before['uptime'],'Unexpected reboot'
  assert state['animationFrames']==7 and a['animationRenderedFrames']>=7,'Animation interrupted'
  assert a['animationFailedLedFrames']==0,a
  maximum=max(maximum,a['animationMaxFrameGap'])
  samples.append({k:state[k] for k in ['freeHeap','maxFreeBlock']})
  if (i+1)%10==0:print({'mode':args.mode,'uploads':i+1,'gap':maximum,'diag':diag,**stats},flush=True)
 # Failed commands must not reach application callbacks or replace the animation.
 rejects=[]
 for payload,expected in [(b'{"bitmapAnimation":{"data":[[65536]]}}' if args.mode=='mqtt' else b'{"setScreen":{"bitmapAnimation":{"data":[[65536]]}}}',(406,)),(b' '*(11300 if args.mode=='mqtt' else 12289),(413,)),(b'{"probe":"'+b'x'*10000,(503,))]:
  old=get('/api/animationinfo')
  rejects.append(send(payload,expected,slow=expected==(413,)))
  current=get('/api/animationinfo')
  assert current['animationRenderedFrames']>=old['animationRenderedFrames']
  if expected==(413,):assert current['animationRenderedFrames']>=old['animationRenderedFrames']+10
  maximum=max(maximum,current['animationMaxFrameGap'])
 time.sleep(2)
 assert seen==expected_frames,(len(seen),len(expected_frames))
 assert stats['previews'] and not any(stats[k] for k in ['parseErrors','wsErrors','wrongPixels']),stats
 assert maximum<=250,maximum
 result={'mode':args.mode,'qos':args.qos if args.mode=='mqtt' else None,'uploads':args.uploads,**stats,'pixelExactFrames':len(seen),'maxGap':maximum,'minSampledHeap':min(s['freeHeap'] for s in samples),'minSampledBlock':min(s['maxFreeBlock'] for s in samples),'minReceiveHeap':min(d['freeHeap'] for d in diags),'minReceiveBlock':min(d['maxBlock'] for d in diags),'lastReceive':diags[-1],'rejections':rejects}
 Path(args.result).write_text(json.dumps(result,indent=2));print('RESULT',json.dumps(result),flush=True)
finally:
 stop.set()
 if 'mqtt' in links:
  links['mqtt'].sendall(b'\xe0\x00');links['mqtt'].close()

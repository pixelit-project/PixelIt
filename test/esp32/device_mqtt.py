"""Opt-in ESP32 MQTT test using a temporary isolated local test broker.
Temporarily changes MQTT configuration, restores it in finally, leaves an animation.
Only the designated device connects; publications use its device screen topic.
"""
import argparse,json,re,socket,threading,time,urllib.request
from pathlib import Path
from device_limits import animation,WebSocket

def remaining(n):
 b=bytearray()
 while True:
  d=n%128;n//=128;b.append(d|(128 if n else 0))
  if not n:return bytes(b)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('url');p.add_argument('--result',required=True);args=p.parse_args();base=args.url.rstrip('/');host=base.split('//')[-1];httpLock=threading.Lock()
 def get(path):
  with httpLock,urllib.request.urlopen(base+path,timeout=15) as r:return json.load(r)
 def post(path,obj):
  request=urllib.request.Request(base+path,json.dumps(obj,separators=(',',':')).encode(),{'Content-Type':'application/json'})
  with httpLock,urllib.request.urlopen(request,timeout=20) as r:
   assert r.status==200
   assert json.load(r)['response']=='OK'
 time.sleep(2)
 deadline=time.monotonic()+30
 while True:
  try:original=get('/api/config');info=get('/api/matrixinfo');break
  except OSError:
   if time.monotonic()>=deadline:raise
   time.sleep(1)
 full=info['maxAnimationPixels']//256;wireLimit=info['maxMqttPacketSize'];assert info['buildSection'].startswith('ESP32') and not original['clockAutoFallbackActive']
 route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);route.connect((host,80));local=route.getsockname()[0];route.close()
 listener=socket.socket();listener.bind((local,0));listener.listen(1);port=listener.getsockname()[1]
 ready=threading.Event();lock=threading.Lock();link={};acks=set();subscriptions=set();errors=[];stop=threading.Event()
 root=Path(__file__).resolve().parents[2];gamma=next((root/'.pio/libdeps').glob('ESP32*/Framebuffer GFX/gamma.h')).read_text()
 def table(name):return [int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+',re.search(name+r'\[\] = \{(.*?)\}',gamma,re.S)[1])]
 g5,g6=table('gamma5'),table('gamma6');seen=set();armed=threading.Event()
 expected={''.join(f'{g5[p>>11]:02X}{g6[(p>>5)&63]:02X}{g5[p&31]:02X}' for p in frame) for frame in animation(full)['bitmapAnimation']['data']}
 topic='pixelit-limit-test/'+info['hostname']+'/setScreen'
 def broker():
  try:
   client,address=listener.accept();assert address[0]==host;link['client']=client
   def read(n):
    b=bytearray()
    while len(b)<n:
     part=client.recv(n-len(b))
     if not part:raise EOFError()
     b.extend(part)
    return bytes(b)
   while not stop.is_set():
    opcode=read(1)[0];length=0;factor=1
    while True:
     d=read(1)[0];length+=(d&127)*factor;factor*=128
     if not d&128:break
    body=read(length);reply=None
    if opcode>>4==1:reply=b'\x20\x02\x00\x00'
    elif opcode>>4==8:
     n=int.from_bytes(body[2:4],'big');subscriptions.add(body[4:4+n].decode());reply=b'\x90\x03'+body[:2]+b'\x00'
     if topic in subscriptions:ready.set()
    elif opcode>>4==12:reply=b'\xd0\x00'
    elif opcode>>4==4:acks.add(int.from_bytes(body,'big'))
    if reply:
     with lock:client.sendall(reply)
  except Exception as e:
   if not stop.is_set():errors.append(repr(e));ready.set()
 threading.Thread(target=broker,daemon=True).start();result={'samples':[]};previews=0;previewErrors=[]
 def preview():
  nonlocal previews
  try:
   ws=WebSocket(host)
   while not stop.is_set():
    m=ws.receive()
    if 'liveview' in m:
     previews+=1
     if armed.is_set():
      frame=m['liveview'].upper();assert frame in expected,'Unexpected MQTT preview pixels';seen.add(frame)
  except Exception as e:
   if not stop.is_set():previewErrors.append(repr(e))
 loadStop=threading.Event();loadErrors=[];loadThread=None;httpUploads=[]
 changed={k:original[k] for k in ['hostname','mqttAktiv','mqttServer','mqttPort','mqttUser','mqttPassword','mqttMasterTopic','mqttUseDeviceTopic','mqttHAdiscoverable']}
 try:
  post('/api/config',{'mqttAktiv':True,'mqttServer':local,'mqttPort':port,'mqttUser':'','mqttPassword':'','mqttMasterTopic':'pixelit-limit-test/','mqttUseDeviceTopic':True,'mqttHAdiscoverable':False})
  assert ready.wait(35) and not errors,(subscriptions,errors)
  threading.Thread(target=preview,daemon=True).start();time.sleep(2)
  post('/api/screen',animation(full));time.sleep(.5);armed.set()
  def concurrent_http():
   try:
    while not loadStop.is_set() and len(httpUploads)<20:
     post('/api/screen',animation(full));httpUploads.append(time.monotonic());loadStop.wait(.8)
   except Exception as e:loadErrors.append(repr(e))
  loadThread=threading.Thread(target=concurrent_http,daemon=True);loadThread.start()
  packetId=0
  def send(obj,qos=1,status=200,slow=False):
   nonlocal packetId
   payload=json.dumps(obj,separators=(',',':')).encode() if not isinstance(obj,bytes) else obj
   packetId+=1;n=len(topic);body=n.to_bytes(2,'big')+topic.encode()+(packetId.to_bytes(2,'big') if qos else b'')+payload
   wire=bytes([0x32 if qos else 0x30])+remaining(len(body))+body
   previous=get('/api/animationinfo')['mqttReceive']['messages']
   with lock:
    if slow:
     for off in range(0,len(wire),1024):link['client'].sendall(wire[off:off+1024]);time.sleep(.04)
    else:link['client'].sendall(wire)
   deadline=time.monotonic()+20
   while time.monotonic()<deadline:
    a=get('/api/animationinfo');d=a['mqttReceive']
    if d['messages']>previous:
     assert d['status']==status,d
     if not qos or packetId in acks:break
    time.sleep(.05)
   else:raise AssertionError('Receive/PUBACK timeout')
   s=get('/api/matrixinfo');assert s['mqttConnected'];assert not errors and not previewErrors,(errors,previewErrors)
   entry={'qos':qos,'payloadBytes':len(payload),'packetBytes':len(wire),'heap':s['freeHeap'],'block':s['maxFreeBlock'],'lowWaterHeap':s['minFreeHeap'],**a};result['samples'].append(entry);print(json.dumps(entry),flush=True);return a
  for qos in [0,1]:
   for i in range(15):
    a=send(animation(full),qos);time.sleep(.3)
    assert a['animationMaxFrameGap']<=200,a
  loadStop.set();loadThread.join(20);assert not loadThread.is_alive() and not loadErrors,loadErrors
  result['concurrentHttpUploads']=len(httpUploads)
  payload=json.dumps(animation(full),separators=(',',':')).encode()
  for qos in [0,1]:
   overhead=1+3+2+len(topic)+(2 if qos else 0)
   send(payload.ljust(wireLimit-overhead,b' '),qos=qos)
  for obj,status in [(b'{"bitmapAnimation":{"data":[[65536]]}}',406),(b' '*wireLimit,413),(b'{"metadata":"'+b'x'*40000,503)]:send(obj,status=status)
  previous=get('/api/animationinfo');a=send(b' '*wireLimit,status=413,slow=True)
  assert a['animationRenderedFrames']>previous['animationRenderedFrames']+10 and a['animationMaxFrameGap']<=200,a
  send(animation(full));previewDeadline=time.monotonic()+full*.15*3+3
  while seen!=expected and time.monotonic()<previewDeadline:time.sleep(.2)
  assert seen==expected,(len(seen),len(expected));result['pixelExactFrames']=len(seen);result.update({'previews':previews,'previewErrors':previewErrors,'final':get('/api/matrixinfo')});assert previews>20
 finally:
  loadStop.set()
  if loadThread:loadThread.join(20)
  stop.set();post('/api/config',changed)
  if 'client' in link:link['client'].close()
  listener.close()
  for attempt in range(30):
   try:
    time.sleep(1);restored=get('/api/config')
    if all(restored[k]==v for k,v in changed.items()):break
   except Exception:pass
  else:raise AssertionError('MQTT configuration restore not verified')
  post('/api/screen',animation(full));result['restored']=True;Path(args.result).write_text(json.dumps(result,indent=2))
 print('PASS',args.result,flush=True)
if __name__=='__main__':main()

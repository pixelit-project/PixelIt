"""Opt-in animation timing test with three full HA discovery republishes.

Temporarily displays a test animation and restores the clock. Writes the existing
clock fallback delay three times to request discovery; settings remain unchanged.
Requires the new firmware, enabled discovery and an accessible MQTT broker.
"""
import argparse
import json, select, socket, struct, time, urllib.request
from pathlib import Path
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('url')
BASE = parser.parse_args().url.rstrip('/')
def get(path):
    with urllib.request.urlopen(BASE+path,timeout=10) as r: return json.load(r)
def screen(data):
    req=urllib.request.Request(BASE+'/api/screen',json.dumps(data,separators=(',',':')).encode(),{'Content-Type':'application/json'})
    with urllib.request.urlopen(req,timeout=15) as r: assert r.status==200

def string(data):
    data=data.encode(); return struct.pack('!H',len(data))+data

def packet(kind,data):
    size=len(data); encoded=b''
    while True:
        digit=size%128; size//=128; encoded+=bytes([digit|(128 if size else 0)])
        if not size: break
    return bytes([kind])+encoded+data

before=get('/api/matrixinfo')
cfg=get('/api/config')
assert cfg.get('mqttAktiv') and cfg.get('mqttHAdiscoverable', True), 'Enable MQTT discovery before this test'
s=socket.create_connection((cfg['mqttServer'],cfg.get('mqttPort',1883)),timeout=10)
flags=2; payload=string('PixelIt-performance-check')
if cfg.get('mqttUser'):
    flags|=128; payload+=string(cfg['mqttUser'])
if cfg.get('mqttPassword'):
    flags|=64; payload+=string(cfg['mqttPassword'])
s.sendall(packet(16,string('MQTT')+bytes([4,flags])+struct.pack('!H',60)+payload))
ack=s.recv(4); assert ack==b'\x20\x02\x00\x00', 'MQTT connection rejected'
# Observe this device's discovery configs so a timing pass cannot hide an
# unprocessed command (for example, because an MQTT buffer allocation failed).
subscription='homeassistant/+/'+before['deviceID']+'/#'
s.sendall(packet(130,struct.pack('!H',1)+string(subscription)+bytes([0])))
received=b''
def drain():
    global received
    count=0
    while select.select([s],[],[],0)[0]:
        data=s.recv(65536)
        assert data, 'Broker disconnected'
        received+=data
    while len(received)>=2:
        size=0; multiplier=1; pos=1
        while pos<len(received):
            digit=received[pos]; pos+=1; size+=(digit&127)*multiplier
            if not digit&128: break
            multiplier*=128
        else: break
        if len(received)<pos+size: break
        if received[0]>>4==3: count+=1
        received=received[pos+size:]
    return count
time.sleep(1)
drain()  # Discard the retained configs delivered on subscription.
discovery_messages=0
base=cfg['mqttMasterTopic']
if not base.endswith('/'): base+='/'
# The master command is subscribed in both topic modes. Write the existing value.
command=json.dumps({'clockAutoFallbackTime':cfg['clockAutoFallbackTime']}).encode()
animation=json.loads((Path(__file__).resolve().parents[2] / 'examples/animation-32x8.json').read_text())
animation['bitmapAnimation']['limitLoops']=0
try:
    screen(animation)
    start=time.monotonic()
    for second in range(1,31):
        time.sleep(max(0,start+second-time.monotonic()))
        if second in (5,15,25):
            s.sendall(packet(48,string(base+'setConfig')+command))
            print('Discovery requested via unchanged clock setting',second,flush=True)
        state=get('/api/animationinfo'); info=get('/api/matrixinfo')
        discovery_messages+=drain()
        print(json.dumps({'second':second,'discoveryMessages':discovery_messages,**state,'uptime':info['uptime'],'freeHeap':info['freeHeap']}),flush=True)
        assert info['uptime']>=before['uptime'], 'Device rebooted'
        assert info['animationFrames']==7 and state['animationDelay']==150, 'Another client replaced the test animation'
        assert state['animationFailedLedFrames']==0, 'LED transfer dropped a frame'
    assert discovery_messages>=96, 'All three discovery republishes must be observed'
    assert state['animationMaxFrameGap']<=400, 'Animation stalled during discovery'
    assert state['animationRenderedFrames']>=180, 'Animation throughput dropped by more than 10%'
    saved=get('/api/config')
    assert saved.get('clockAutoFallbackTime')==cfg['clockAutoFallbackTime'], 'Clock delay changed'
    assert 'mqttHAdiscoverable' in saved and 'checkUpdateScreen' in saved, 'Saved configuration is incomplete'
    print('Stress summary:',json.dumps(state),flush=True)
finally:
    try: s.sendall(packet(224,b''))
    finally: s.close(); screen({'clock':{}})

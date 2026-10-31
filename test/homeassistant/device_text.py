"""Opt-in device test: stage HA text options during playback, then show together.

Temporarily replaces the screen and restores the clock. Does not write config.
Usage: python3 test/homeassistant/device_text.py http://192.168.0.40
"""
import argparse
import json
from pathlib import Path
import socket
import select
import struct
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('url')
base = parser.parse_args().url.rstrip('/')


def get(path):
    with urllib.request.urlopen(base + path, timeout=8) as response:
        return json.load(response)


def screen(payload):
    request = urllib.request.Request(base + '/api/screen', json.dumps(payload, separators=(',', ':'), ensure_ascii=False).encode(),
                                     {'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=15) as response:
        assert response.status == 200


def string(value):
    value = value.encode()
    return struct.pack('!H', len(value)) + value


def packet(kind, payload):
    remaining = len(payload)
    header = bytes([kind])
    while True:
        digit = remaining % 128
        remaining //= 128
        header += bytes([digit | (128 if remaining else 0)])
        if not remaining:
            return header + payload


initial = get('/api/matrixinfo')
config = get('/api/config')
assert 'textFont' in initial, 'Install the HA text settings firmware first'
client = socket.create_connection((config['mqttServer'], config.get('mqttPort', 1883)), timeout=8)
flags = 2
payload = string('PixelIt-text-settings-test')
if config.get('mqttUser'):
    flags |= 128
    payload += string(config['mqttUser'])
if config.get('mqttPassword'):
    flags |= 64
    payload += string(config['mqttPassword'])
client.sendall(packet(16, string('MQTT') + bytes([4, flags]) + struct.pack('!H', 60) + payload))
ack = b''
while len(ack) < 4:
    data = client.recv(4 - len(ack))
    assert data, 'Broker disconnected'
    ack += data
assert ack == b'\x20\x02\x00\x00', 'MQTT connection rejected'
topic = config['mqttMasterTopic'].rstrip('/') + '/'

# Verify the retained discovery payloads actually received by Home Assistant.
client.sendall(packet(130, struct.pack('!H', 1) +
                      string('homeassistant/+/' + initial['deviceID'] + '/#') + bytes([0])))
received = b''
discovered = {}
expected = {'DisplayText', 'TextColor', 'TextFont', 'TextAlignment', 'TextScroll', 'ShowText', 'TextColorPicker', 'FirmwareUpdate'}
deadline = time.monotonic() + 8
while time.monotonic() < deadline and not expected.issubset(discovered):
    if not select.select([client], [], [], .2)[0]:
        continue
    received += client.recv(65536)
    while len(received) >= 2:
        size, multiplier, offset = 0, 1, 1
        while offset < len(received):
            digit = received[offset]
            offset += 1
            size += (digit & 127) * multiplier
            if not digit & 128:
                break
            multiplier *= 128
        else:
            break
        if len(received) < offset + size:
            break
        body = received[offset:offset + size]
        if received[0] >> 4 == 3:
            topic_length = struct.unpack('!H', body[:2])[0]
            value = body[2 + topic_length:]
            if value:
                entity = json.loads(value)
                identifier = entity['unique_id'].removeprefix(initial['deviceID'])
                discovered[identifier] = entity
        received = received[offset + size:]
assert expected.issubset(discovered), 'Text discovery entities are missing'
assert discovered['DisplayText']['command_topic'].endswith('/setTextOptions/text')
assert discovered['ShowText']['command_topic'].endswith('/showText')
assert discovered['TextColorPicker']['command_topic'].endswith('/setTextOptions/colorPicker')
assert discovered['FirmwareUpdate']['device_class'] == 'firmware'
assert 'command_topic' not in discovered['FirmwareUpdate']
print('Text controls, color picker and read-only firmware discovery confirmed on the broker')


def publish(command, value):
    body = json.dumps(value, ensure_ascii=False).encode() if isinstance(value, dict) else value.encode()
    client.sendall(packet(48, string(topic + command) + body))


def wait_for(predicate):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        state = get('/api/matrixinfo')
        assert state['uptime'] >= initial['uptime'], 'Device rebooted'
        if predicate(state):
            return state
        time.sleep(.15)
    raise AssertionError('Device did not confirm the expected text state: ' + repr({key: state.get(key) for key in ('textMessage','textColor','textFont','textAlignment','textScroll','maxFreeBlock','freeHeap','uptime','mqttConnected')}))


options = {'text': 'Hallo "HA" äöü gj', 'color': '#12ABCD', 'font': 'Large',
           'alignment': 'Left', 'scroll': 'Off'}
restore = dict(zip(('text', 'color', 'font', 'alignment', 'scroll'),
                   (initial[key] for key in ('textMessage', 'textColor', 'textFont', 'textAlignment', 'textScroll'))))
try:
    animation = json.loads((Path(__file__).resolve().parents[2] / 'examples/animation-32x8.json').read_text())
    animation['bitmapAnimation']['limitLoops'] = 0
    screen(animation)
    for key, value in options.items():
        publish('setTextOptions/' + key, value)
    state = wait_for(lambda value: value['textMessage'] == options['text'] and
                     (value['textColor'], value['textFont'], value['textAlignment'], value['textScroll']) ==
                     ('#12ABCD', 'Large', 'Left', 'Off'))
    assert state['displayText'] == initial['displayText'], 'Staging displayed the message early'
    assert (state['textColor'], state['textFont'], state['textAlignment'], state['textScroll']) == ('#12ABCD', 'Large', 'Left', 'Off')
    first = get('/api/animationinfo')['animationRenderedFrames']
    time.sleep(1)
    timing = get('/api/animationinfo')
    assert timing['animationRenderedFrames'] > first, 'Staging stopped playback'
    assert timing['animationFailedLedFrames'] == 0
    publish('setTextOptions', {'text': 'invalid update', 'color': 'red'})
    time.sleep(.3)
    assert get('/api/matrixinfo')['textMessage'] == options['text'], 'Invalid update changed the text'
    publish('setTextOptions/colorPicker', '#FF8000')
    wait_for(lambda value: value['textColor'] == '#FF8000')
    publish('setTextOptions/colorPicker', 'OFF')
    wait_for(lambda value: value['textColor'] == '#000000')
    publish('setTextOptions/colorPicker', 'ON')
    wait_for(lambda value: value['textColor'] == '#FF8000')
    publish('setTextOptions/color', '#12ABCD')
    wait_for(lambda value: value['textColor'] == '#12ABCD')
    print('Color picker and HEX field synchronization confirmed')
    publish('showText', 'PRESS')
    wait_for(lambda value: value['displayText'] == options['text'] and not value['sleepMode'])
    print('Staged text, quotes/umlauts, color, large font, left alignment and Off confirmed')
    for key, value in {'font': 'Small', 'alignment': 'Centered', 'scroll': 'Always'}.items():
        publish('setTextOptions/' + key, value)
    wait_for(lambda value: value['textFont'] == 'Small' and value['textAlignment'] == 'Centered' and value['textScroll'] == 'Always')
    publish('showText', 'PRESS')
    time.sleep(.3)
    assert get('/api/config') == config, 'Text controls wrote configuration'
    print('Small font, centered alignment, Always and unchanged config confirmed')
finally:
    try:
        publish('setText', initial['displayText'])
        publish('setTextOptions', restore)
        wait_for(lambda value: value['textMessage'] == restore['text'])
        screen({'clock': {}})
    finally:
        client.sendall(packet(224, b''))
        client.close()
print('Text device checks passed; initial options and clock restored')

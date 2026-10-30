"""Opt-in LED playback timing regression against a physical PixelIt.

Usage: python3 test/animation/device_timing.py http://192.168.0.40
Replaces the display for 60 seconds, then restores the clock. Requires firmware
with frame timing and FastLED transfer counters; never changes configuration.
"""
import argparse
import json
from pathlib import Path
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('url')
parser.add_argument('--seconds', type=int, default=60)
args = parser.parse_args()
base = args.url.rstrip('/')


def info():
    with urllib.request.urlopen(base + '/api/matrixinfo', timeout=10) as response:
        state = json.load(response)
    with urllib.request.urlopen(base + '/api/animationinfo', timeout=10) as response:
        state.update(json.load(response))
    return state


def screen(payload):
    request = urllib.request.Request(
        base + '/api/screen',
        json.dumps(payload, separators=(',', ':')).encode(),
        {'Content-Type': 'application/json'},
    )
    with urllib.request.urlopen(request, timeout=15) as response:
        assert response.status == 200


animation = json.loads((Path(__file__).resolve().parents[2] /
                        'examples/animation-32x8.json').read_text())
animation['bitmapAnimation']['limitLoops'] = 0
before = info()
try:
    screen(animation)
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        time.sleep(min(5, max(0, deadline - time.monotonic())))
        state = info()
        print({key: state[key] for key in (
            'animationRenderedFrames', 'animationMaxFrameGap',
            'animationFailedLedFrames', 'ledFrameRetries', 'slowestTask', 'slowestTaskMillis')}, flush=True)
        assert state['uptime'] >= before['uptime'], 'Device rebooted'
        assert state['animationFailedLedFrames'] == 0, 'LED transfer dropped a frame'
        assert state['animationMaxFrameGap'] <= 400, 'Animation stalled'
    assert state['animationRenderedFrames'] >= args.seconds * 1000 / 150 * 0.9
finally:
    screen({'clock': {}})
print('LED transfer and timing checks passed; clock restored')

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFileSync } = require('child_process');
const install = require('../public/preview-visibility.js');
class Events {
  constructor() { this.events = {}; }
  addEventListener(type, fn) { (this.events[type] || (this.events[type] = [])).push(fn); }
  emit(type, event = {}) { (this.events[type] || []).forEach(fn => fn(event)); }
}
class Native extends Events {
  constructor(url, protocols) { super(); this.url = url; this.protocols = protocols; this.readyState = Native.OPEN; this.sent = []; }
  send(value) { this.sent.push(JSON.parse(value)); }
  close() { this.readyState = Native.CLOSED; this.emit('close'); }
}
Object.assign(Native, { CONNECTING: 0, OPEN: 1, CLOSING: 2, CLOSED: 3 });
const doc = new Events(); doc.hidden = false;
const win = new Events(); Object.assign(win, { WebSocket: Native, URL, atob, location: { href: 'http://display.local/' } });
install(win, doc); const Wrapped = win.WebSocket;
assert.strictEqual(Wrapped.OPEN, Native.OPEN);
let ws = new Wrapped('ws://display.local:81', ['test']);
assert(ws instanceof Native && ws instanceof Wrapped); assert.deepStrictEqual(ws.protocols, ['test']);
ws.emit('message', { data: '{"liveview":"FFFFFF"}' }); assert.strictEqual(ws.sent.length, 0);
ws.emit('message', { data: '{"sysinfo":{}}' }); assert.strictEqual(ws.sent.length, 0);
ws.emit('message', { data: '{"sysinfo":{"previewControl":"true"}}' }); assert.strictEqual(ws.sent.length, 0);
const capability = { data: '{"sysinfo":{"previewControl":true}}' };
ws.emit('message', capability); ws.emit('message', capability);
assert.deepStrictEqual(ws.sent, [{ preview: { enabled: true } }]);
doc.hidden = true; doc.emit('visibilitychange'); doc.emit('visibilitychange');
assert.deepStrictEqual(ws.sent[1], { preview: { enabled: false } }); assert.strictEqual(ws.sent.length, 2);
doc.hidden = false; doc.emit('visibilitychange'); assert.strictEqual(ws.sent[2].preview.enabled, true);
win.emit('pagehide'); assert.strictEqual(ws.sent[3].preview.enabled, false);
win.emit('pageshow'); assert.strictEqual(ws.sent[4].preview.enabled, true);
ws.close(); doc.hidden = true; doc.emit('visibilitychange'); assert.strictEqual(ws.sent.length, 5);
ws = new Wrapped('ws://display.local:81'); ws.emit('message', capability);
assert.deepStrictEqual(ws.sent, [{ preview: { enabled: false } }]);
const other = new Wrapped('ws://display.local:80'); other.emit('message', capability); assert.strictEqual(other.sent.length, 0);
install(win, doc); assert.strictEqual(win.WebSocket, Wrapped); assert.strictEqual(doc.events.visibilitychange.length, 1);
doc.hidden = false; doc.emit('visibilitychange');
const replacement = new Wrapped('ws://display.local:81'); replacement.emit('message', capability);
assert.strictEqual(ws.sent[ws.sent.length - 1].preview.enabled, false);
assert.strictEqual(replacement.sent[0].preview.enabled, true);
ws.emit('message', capability); assert.strictEqual(ws.sent[ws.sent.length - 1].preview.enabled, false);
replacement.close(); assert.strictEqual(ws.sent[ws.sent.length - 1].preview.enabled, true);
const compact = new Wrapped('ws://compact.local:81');
compact.emit('message', { data: '{"sysinfo":{"previewControl":true,"previewCompact":true}}' });
assert.deepStrictEqual(compact.sent, [{ preview: { enabled: true, compact: true } }]);
const frame = { data: '{"liveview64":"AfD/"}' };
compact.emit('message', frame);
assert.strictEqual(frame.data, '{"liveview":"01F0FF"}');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'preview-index-'));
try {
  const original = '<html><head><script src="https://example.test/published.js"></script></head></html>';
  const input = path.join(dir, 'input.html'), output = path.join(dir, 'output.html');
  fs.writeFileSync(input, original);
  const script = path.join(__dirname, '../scripts/preview-compatible-index.cjs');
  execFileSync(process.execPath, [script, input, output]);
  const first = fs.readFileSync(output, 'utf8');
  assert(first.includes('https://example.test/published.js'));
  execFileSync(process.execPath, [script, output, output]);
  assert.strictEqual(fs.readFileSync(output, 'utf8'), first);
} finally { fs.rmSync(dir, { recursive: true, force: true }); }
console.log('Preview visibility: capabilities, hidden/visible, reconnect/close, static API and published-index embedding passed');

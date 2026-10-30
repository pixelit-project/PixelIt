const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const source = fs.readFileSync(require('path').join(__dirname, '../src/views/TestArea.vue'), 'utf8')
    .split('<script>')[1].split('</script>')[0].replace(/^import .*;$/gm, '').replace('export default', 'this.component =');
const context = { actionFeedback: {}, ButtonCondition: {}, PrismEditor: {}, Liveview: {} };
vm.runInNewContext(source, context);
let sent;
const editor = { testarea: { image: '' }, $socket: { sendObj: payload => { sent = payload; } } };
function send(input) { editor.testarea.image = input; context.component.methods.sendBitmap.call(editor); return sent.setScreen; }
for (const [size, limit, width] of [[64, 28, 8], [256, 7, 32]]) {
    const frame = Array(size).fill(65535);
    const single = send(JSON.stringify(frame));
    assert.equal((single.bitmap || single.bitmapAnimation).size.width, width);
    assert.equal(sent.forcedDuration, 5000);
    for (const text of [JSON.stringify(frame) + ',' + JSON.stringify(frame), JSON.stringify([frame, frame]), JSON.stringify(frame) + ',\n']) {
        const result = send(text);
        assert.equal((result.bitmap || result.bitmapAnimation).size.width, width);
    }
    const maximum = Array(limit).fill(JSON.stringify(frame)).join(',');
    assert.equal(send(maximum).bitmapAnimation.data.length, limit);
    assert.throws(() => send(maximum + ',' + JSON.stringify(frame)), /Maximum/);
}
assert.throws(() => send('[1,2]'), /64.*256/);
assert.throws(() => send(JSON.stringify([Array(64).fill(0), Array(256).fill(0)])), /same size/);
for (const value of [-1, 65536, 1.5, '0', null]) {
    assert.throws(() => send(JSON.stringify(Array(256).fill(value))), /RGB565/);
}
assert.throws(() => send('[]'), /64.*256/);
assert.throws(() => send('invalid'));
console.log('Test Area: automatic dimensions, static and animated bitmaps, input formats, frame limits and RGB565 validation passed.');

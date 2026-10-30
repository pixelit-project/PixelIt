const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const Vue = require('vue');
const storage = new Map();
let intervalDelay;
const context = {
    ButtonSave: {},
    localStorage: { getItem: key => storage.get(key), setItem: (key, value) => storage.set(key, value) },
    setInterval: (callback, delay) => { intervalDelay = delay; return 1; }, clearInterval() {},
};
const source = fs.readFileSync(require('path').join(__dirname, '../src/views/Creator.vue'), 'utf8')
    .split('<script>')[1].split('</script>')[0].replace(/^import .*;$/gm, '').replace('export default', 'this.component =');
vm.runInNewContext(source, context);
function create() {
    const editor = new Vue(context.component);
    editor.$store = { state: { socket: { isConnected: true } } };
    editor.sent = [];
    editor.$socket = { sendObj: value => editor.sent.push(JSON.parse(JSON.stringify(value))) };
    return editor;
}
(async () => {
    const e = create();
    e.colors = '#ff0000ff'; e.paintPixel(0); await Vue.nextTick();
    assert.equal(e.frames[0][0], 63488);
    assert.equal(e.undoStack.length, 1, 'String synchronization must not duplicate history');
    e.undo(); await Vue.nextTick(); assert.equal(e.frames[0][0], 0);
    e.redo(); await Vue.nextTick(); assert.equal(e.frames[0][0], 63488);
    e.tool = 'picker'; e.paintPixel(0); assert.equal(e.colors, '#ff0000'); assert.equal(e.tool, 'pen');
    e.tool = 'eraser'; e.paintPixel(0); await Vue.nextTick(); assert.equal(e.frames[0][0], 0);
    e.tool = 'fill'; e.colors = '#00ff00'; e.paintPixel(0); await Vue.nextTick();
    assert(e.frames[0].every(pixel => pixel === 2016));
    e.clearFrame(); await Vue.nextTick(); assert(e.frames[0].every(pixel => pixel === 0));
    e.undo(); await Vue.nextTick(); assert(e.frames[0].every(pixel => pixel === 2016));
    // Flood fill must not wrap across row boundaries or cross differently colored pixels.
    e.frames = [Array(64).fill(63488)]; e.frames[0][7] = 0; e.frames[0][8] = 0;
    e.colors = '#0000ff'; e.paintPixel(7); await Vue.nextTick();
    assert.equal(e.frames[0][7], 31); assert.equal(e.frames[0][8], 0);
    e.addFrame(true); await Vue.nextTick(); assert.equal(e.frames.length, 2);
    e.moveFrame(-1); await Vue.nextTick(); assert.equal(e.selectedFrame, 0);
    e.undo(); await Vue.nextTick(); assert.equal(e.selectedFrame, 1);
    e.deleteFrame(); await Vue.nextTick(); assert.equal(e.frames.length, 1);
    e.undo(); await Vue.nextTick(); assert.equal(e.frames.length, 2);
    e.onionSkin = true; e.previewDelay = 350; await Vue.nextTick();
    const restored = create(); restored.restoreDraft(); await Vue.nextTick();
    assert.equal(JSON.stringify(restored.frames), JSON.stringify(e.frames));
    assert.equal(restored.selectedFrame, e.selectedFrame); assert.equal(restored.previewDelay, 350); assert(restored.onionSkin);
    e.livedraw = true; e.togglePreview(); assert(e.playing); assert.equal(intervalDelay, 350);
    assert.equal(e.sent.at(-1).setScreen.bitmapAnimation.animationDelay, 350);
    assert.equal(e.sent.at(-1).setScreen.bitmapAnimation.data.length, 2);
    e.previewFrame = 1; e.togglePreview(); assert(!e.playing); assert.equal(e.selectedFrame, 1);
    assert.equal(e.sent.at(-1).setScreen.bitmapAnimation.data.length, 1);
    assert.deepEqual(e.sent.at(-1).setScreen.bitmapAnimation.data[0], JSON.parse(JSON.stringify(e.frames[1])));
    e.livedraw = false; const count = e.sent.length; e.togglePreview(); e.togglePreview(); assert.equal(e.sent.length, count);
    e.array8x8String = '[1,2]'; await Vue.nextTick(); assert(e.animationError);
    e.array8x8String = JSON.stringify(Array(64).fill(42)); await Vue.nextTick(); assert(!e.animationError); assert.equal(e.frames[0][0], 42);
    e.undo(); await Vue.nextTick(); assert.equal(e.frames.length, 2);
    const originalSet = context.localStorage.setItem;
    context.localStorage.setItem = () => { throw Error('quota'); };
    e.saveDraft(); assert(e.draftStatus.includes('could not be saved'));
    context.localStorage.setItem = originalSet;
    for (let value = 1; value <= 110; value++) { e.colors = '#' + value.toString(16).padStart(2, '0') + 'ffff'; e.paintPixel(0); }
    assert(e.undoStack.length <= 100);
    const wide = create(); wide.changePixelModeTo8x32();
    wide.colors = '#ff0000'; wide.paintPixel(31); await Vue.nextTick();
    assert.equal(wide.bitmap32[31], 63488); assert.equal(wide.undoStack32.length, 1);
    assert.equal(wide.undoStack.length, 0, '8×32 history must stay separate');
    wide.undo32(); await Vue.nextTick(); assert.equal(wide.bitmap32[31], 0);
    wide.redo32(); await Vue.nextTick(); assert.equal(wide.bitmap32[31], 63488);
    wide.tool = 'picker'; wide.paintPixel(31); assert.equal(wide.colors, '#ff0000');
    wide.tool = 'eraser'; wide.paintPixel(31); await Vue.nextTick(); assert.equal(wide.bitmap32[31], 0);
    wide.bitmap32 = Array(256).fill(63488); wide.bitmap32[31] = 0; wide.bitmap32[32] = 0;
    wide.tool = 'fill'; wide.colors = '#0000ff'; wide.paintPixel(31); await Vue.nextTick();
    assert.equal(wide.bitmap32[31], 31); assert.equal(wide.bitmap32[32], 0, 'Fill must not wrap rows');
    wide.clearBitmap32(); await Vue.nextTick(); assert(wide.bitmap32.every(pixel => pixel === 0));
    wide.undo32(); await Vue.nextTick(); assert.equal(wide.bitmap32[31], 31);
    wide.livedraw = true; wide.tool = 'pen'; wide.colors = '#00ff00'; wide.paintPixel(255); await Vue.nextTick();
    const message = wide.sent.at(-1).setScreen;
    assert(!message.bitmapAnimation); assert.equal(message.bitmap.size.width, 32); assert.equal(message.bitmap.size.height, 8);
    assert.equal(message.bitmap.data.length, 256); assert.equal(message.bitmap.data[255], 2016);
    const recoveredWide = create(); recoveredWide.restoreDraft32(); await Vue.nextTick();
    assert.equal(JSON.stringify(recoveredWide.bitmap32), JSON.stringify(wide.bitmap32));
    assert.equal(JSON.parse(storage.get('pixelit-creator-8x8-v1')).frames.length, 2, 'Wide draft must not replace animation draft');
    wide.array8x32String = '[1,2]'; await Vue.nextTick(); assert(wide.bitmap32Error);
    wide.array8x32String = JSON.stringify([Array(256).fill(0), Array(256).fill(0)]); await Vue.nextTick(); assert(wide.bitmap32Error);
    wide.array8x32String = JSON.stringify(Array(256).fill(42)); await Vue.nextTick(); assert(!wide.bitmap32Error);
    wide.undo32(); await Vue.nextTick(); assert.equal(wide.bitmap32[255], 2016);
    wide.changePixelModeTo8x8(); assert.equal(wide.frames[0][0], 0);
    wide.changePixelModeTo8x32(); assert.equal(wide.bitmap32[255], 2016);
    wide.$destroy(); recoveredWide.$destroy();
    e.$destroy(); restored.$destroy();
    console.log('Creator: tools, flood-fill boundaries, undo/redo, frame operations, imports, draft recovery, storage failures and live play/pause passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });

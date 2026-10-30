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
    assert.equal(wide.frames[wide.selectedFrame][31], 63488); assert.equal(wide.undoStack.length, 1);
    assert.equal(wide.canvases[0].undoStack.length, 0, '8×32 history must stay separate');
    wide.undo(); await Vue.nextTick(); assert.equal(wide.frames[wide.selectedFrame][31], 0);
    wide.redo(); await Vue.nextTick(); assert.equal(wide.frames[wide.selectedFrame][31], 63488);
    wide.tool = 'picker'; wide.paintPixel(31); assert.equal(wide.colors, '#ff0000');
    wide.tool = 'eraser'; wide.paintPixel(31); await Vue.nextTick(); assert.equal(wide.frames[wide.selectedFrame][31], 0);
    wide.frames[wide.selectedFrame] = Array(256).fill(63488); wide.frames[wide.selectedFrame][31] = 0; wide.frames[wide.selectedFrame][32] = 0;
    wide.tool = 'fill'; wide.colors = '#0000ff'; wide.paintPixel(31); await Vue.nextTick();
    assert.equal(wide.frames[wide.selectedFrame][31], 31); assert.equal(wide.frames[wide.selectedFrame][32], 0, 'Fill must not wrap rows');
    wide.clearFrame(); await Vue.nextTick(); assert(wide.frames[wide.selectedFrame].every(pixel => pixel === 0));
    wide.undo(); await Vue.nextTick(); assert.equal(wide.frames[wide.selectedFrame][31], 31);
    wide.livedraw = true; wide.tool = 'pen'; wide.colors = '#00ff00'; wide.paintPixel(255); await Vue.nextTick();
    const message = wide.sent.at(-1).setScreen;
    assert(!message.bitmapAnimation); assert.equal(message.bitmap.size.width, 32); assert.equal(message.bitmap.size.height, 8);
    assert.equal(message.bitmap.data.length, 256); assert.equal(message.bitmap.data[255], 2016);
    const recoveredWide = create(); recoveredWide.changePixelModeTo8x32(); recoveredWide.restoreDraft(); await Vue.nextTick();
    assert.equal(JSON.stringify(recoveredWide.frames[0]), JSON.stringify(wide.frames[wide.selectedFrame]));
    assert.equal(JSON.parse(storage.get('pixelit-creator-8x8-v1')).frames.length, 2, 'Wide draft must not replace animation draft');
    wide.array8x32String = '[1,2]'; await Vue.nextTick(); assert(wide.animationError);
    wide.array8x32String = JSON.stringify([Array(256).fill(0), Array(256).fill(0)]); await Vue.nextTick(); assert(wide.animationError);
    wide.array8x32String = JSON.stringify(Array(256).fill(42)); await Vue.nextTick(); assert(!wide.animationError);
    wide.undo(); await Vue.nextTick(); assert.equal(wide.frames[wide.selectedFrame][255], 2016);
    wide.changePixelModeTo8x8(); assert.equal(wide.frames[0][0], 0);
    wide.changePixelModeTo8x32(); assert.equal(wide.frames[wide.selectedFrame][255], 2016);
    wide.addFrame(true); await Vue.nextTick(); assert.equal(wide.frames.length, 2);
    wide.colors = '#ff0000'; wide.paintPixel(0); await Vue.nextTick();
    assert.equal(wide.frames[0][0], 63488); assert.equal(wide.frames[1][0], 63488);
    wide.tool = 'eraser'; wide.paintPixel(0); await Vue.nextTick();
    assert.equal(wide.frames[0][0], 63488); assert.equal(wide.frames[1][0], 0, 'Wide frames must be independent');
    wide.moveFrame(-1); await Vue.nextTick(); assert.equal(wide.selectedFrame, 0);
    assert.equal(wide.frames[0][0], 0);
    wide.undo(); await Vue.nextTick(); assert.equal(wide.selectedFrame, 1);
    wide.previewDelay = 450; wide.onionSkin = true; await Vue.nextTick();
    wide.togglePreview(); assert.equal(intervalDelay, 450); assert(wide.playing);
    assert.equal(wide.sent.at(-1).setScreen.bitmapAnimation.size.width, 32);
    assert.equal(wide.sent.at(-1).setScreen.bitmapAnimation.data.length, 2);
    assert.equal(wide.sent.at(-1).setScreen.bitmapAnimation.data[0].length, 256);
    wide.previewFrame = 1; wide.togglePreview();
    assert(!wide.sent.at(-1).setScreen.bitmapAnimation); assert.equal(wide.sent.at(-1).setScreen.bitmap.data.length, 256);
    const wideRestored = create(); wideRestored.changePixelModeTo8x32(); wideRestored.restoreDraft(); await Vue.nextTick();
    assert.equal(JSON.stringify(wideRestored.frames), JSON.stringify(wide.frames));
    assert.equal(wideRestored.previewDelay, 450); assert(wideRestored.onionSkin);
    wide.array8x32String = Array(2).fill(JSON.stringify(Array(256).fill(31))).join(',\n'); await Vue.nextTick();
    assert(!wide.animationError); assert.equal(wide.frames.length, 2); assert.equal(wide.frames[0][0], 31);
    wide.undo(); await Vue.nextTick(); assert.equal(wide.frames[0][0], 63488);
    wide.togglePreview(); wide.changePixelModeTo8x8(); assert(!wide.playing);
    assert.equal(wide.sent.at(-1).setScreen.bitmap.size.width, 32, 'Mode switch must pause the old canvas');
    wide.changePixelModeTo8x32(); assert.equal(wide.frames.length, 2);
    // Migrate the old static draft without deleting it.
    storage.delete('pixelit-creator-8x32-v2');
    storage.set('pixelit-creator-8x32-v1', JSON.stringify(Array(256).fill(123)));
    const migrated = create(); migrated.restoreDraft(1); await Vue.nextTick(); migrated.changePixelModeTo8x32();
    assert.equal(migrated.frames.length, 1); assert.equal(migrated.frames[0][0], 123);
    assert(storage.has('pixelit-creator-8x32-v1')); assert(storage.has('pixelit-creator-8x32-v2'));
    // The shared pointer reorder path also works for 256-pixel frames.
    migrated.addFrame(false); await Vue.nextTick();
    const dragTarget = { getBoundingClientRect: () => ({ left: 0, top: 0 }), setPointerCapture() {}, hasPointerCapture: () => false };
    migrated.startFrameDrag({ button: 0, isPrimary: true, currentTarget: dragTarget, pointerId: 1, clientX: 0, clientY: 0 }, 1);
    migrated.draggedFrame = 1; migrated.dropPosition = 0;
    migrated.dropFrame({ currentTarget: dragTarget, pointerId: 1 }); await Vue.nextTick();
    assert.equal(migrated.selectedFrame, 0); assert.equal(migrated.frames[0][0], 0); assert.equal(migrated.frames[1][0], 123);
    migrated.undo(); await Vue.nextTick(); assert.equal(migrated.frames[0][0], 123);
    // Save and gallery playback must preserve wide animation dimensions and all frames.
    const loadComponent = (file, env = {}) => {
        const componentSource = fs.readFileSync(require('path').join(__dirname, '..', file), 'utf8')
            .split('<script>')[1].split('</script>')[0].replace(/^import .*;$/gm, '').replace('export default', 'this.component =');
        const sandbox = { ...env }; vm.runInNewContext(componentSource, sandbox); return sandbox.component;
    };
    let upload;
    const save = loadComponent('src/components/ButtonBMPSave.vue', {
        notify() {}, fetch: async (url, options) => { upload = JSON.parse(options.body); return { ok: true }; },
    });
    const dialog = new Vue({ ...save, propsData: { data: migrated.array8x32String, pixelMode: 1, condition: true },
        beforeCreate() { this.$cookies = { get: () => '' }; },
    });
    dialog.isValid = true; dialog.bitmapName = 'Wide animation'; await dialog.agreeWraper();
    assert.equal(upload.sizeX, 32); assert.equal(upload.sizeY, 8); assert(upload.animated);
    assert.equal(JSON.parse('[' + upload.rgb565array + ']')[1].length, 256); assert(!('animationDelay' in upload));
    const gallery = loadComponent('src/views/Gallery.vue', { BmpCanvas: {}, BmpDialog: {}, actionFeedback: {} });
    let playback;
    const receiver = { $socket: { sendObj: value => { playback = value.setScreen; } } };
    gallery.methods.sendBitmap.call(receiver, migrated.array8x32String, 32);
    assert.equal(playback.bitmapAnimation.size.width, 32); assert.equal(playback.bitmapAnimation.data.length, 2);
    gallery.methods.sendBitmap.call(receiver, JSON.stringify(Array(256).fill(0)), 32);
    assert(playback.bitmap); assert(!playback.bitmapAnimation);
    for (const [mode, limit, size] of [[0, 28, 64], [1, 7, 256]]) {
        const limited = create();
        if (mode) limited.changePixelModeTo8x32();
        for (let i = 1; i < limit; i++) limited.addFrame(i % 2 === 0);
        await Vue.nextTick();
        assert.equal(limited.frames.length, limit); assert.equal(limited.frameLimit, limit);
        const historyLength = limited.undoStack.length;
        limited.addFrame(false); limited.addFrame(true); assert.equal(limited.frames.length, limit);
        assert.equal(limited.undoStack.length, historyLength);
        limited.deleteFrame(); limited.addFrame(false); await Vue.nextTick(); assert.equal(limited.frames.length, limit);
        const excess = Array(limit + 1).fill(JSON.stringify(Array(size).fill(0))).join(',');
        limited.bitmapText = excess; await Vue.nextTick(); assert(limited.animationError);
        assert.equal(limited.frames.length, limit, 'Oversized imports must preserve existing frames');
        limited.serializeFrames(); await Vue.nextTick(); assert(!limited.animationError);
        limited.togglePreview(); assert(limited.playing); limited.togglePreview();
        const key = mode === 0 ? 'pixelit-creator-8x8-v1' : 'pixelit-creator-8x32-v2';
        storage.set(key, JSON.stringify({ frames: Array.from({length: limit + 1}, () => Array(size).fill(0)) }));
        limited.restoreDraft(); await Vue.nextTick();
        assert.equal(limited.frames.length, limit + 1, 'Old oversized drafts must remain recoverable without truncation');
        assert(limited.frameLimitExceeded); limited.togglePreview(); assert(!limited.playing);
        limited.livedraw = true; limited.sendLiveFrame(0); assert.equal(limited.sent.length, 0);
        limited.deleteFrame(); await Vue.nextTick(); assert(!limited.frameLimitExceeded);
        limited.$destroy();
    }
    for (const mode of [0, 1]) {
        const painter = create();
        if (mode) painter.changePixelModeTo8x32();
        const width = painter.canvasWidth;
        painter.colors = '#ff0000';
        const target = { setPointerCapture() {}, getBoundingClientRect: () => ({ left: 0, top: 0, width: width * 10, height: 80 }) };
        const down = { button: 0, isPrimary: true, pointerId: 5, preventDefault() {}, currentTarget: target,
            target: { closest: () => ({ dataset: { pixelIndex: '0' } }) } };
        const move = { pointerId: 5, buttons: 1, preventDefault() {}, currentTarget: target, clientX: width * 10 - 5, clientY: 5 };
        painter.startPaintStroke(down); painter.continuePaintStroke(move); await Vue.nextTick();
        assert(painter.frames[0].slice(0, width).every(pixel => pixel === 63488), 'Fast mouse strokes must fill skipped pixels');
        assert.equal(painter.undoStack.length, 1, 'A complete stroke must be one undo step');
        painter.endPaintStroke({ pointerId: 99 }); assert(painter.paintStroke, 'Other pointers must not stop a stroke');
        painter.endPaintStroke({ pointerId: 5 }); painter.undo(); await Vue.nextTick();
        assert(painter.frames[0].every(pixel => pixel === 0));
        painter.redo(); await Vue.nextTick(); assert.equal(painter.frames[0][width - 1], 63488);
        painter.tool = 'eraser'; painter.startPaintStroke(down); painter.continuePaintStroke(move); painter.endPaintStroke(); await Vue.nextTick();
        assert(painter.frames[0].every(pixel => pixel === 0));
        painter.undo(); await Vue.nextTick(); assert.equal(painter.frames[0][width - 1], 63488);
        painter.startPaintStroke({ ...down, button: 2 }); assert(!painter.paintStroke);
        painter.tool = 'picker'; painter.startPaintStroke(down); assert(!painter.paintStroke); assert.equal(painter.tool, 'pen');
        painter.colors = '#00ff00'; painter.startPaintStroke(down);
        painter.continuePaintStroke({ ...move, buttons: 0 }); assert(!painter.paintStroke);
        painter.startPaintStroke(down); painter.continuePaintStroke({ ...move, clientX: -20 }); assert.equal(painter.paintStroke.last, null);
        painter.continuePaintStroke({ ...move, clientY: 75 }); assert.equal(painter.frames[0][width * 8 - 1], 2016);
        painter.endPaintStroke();
        const length = painter.undoStack.length;
        painter.keyboardPaint({ detail: 1 }, 1); assert.equal(painter.undoStack.length, length, 'Pointer clicks must not paint twice');
        painter.keyboardPaint({ detail: 0 }, 1); assert.equal(painter.frames[0][1], 2016);
        painter.$destroy();
    }
    dialog.$destroy(); migrated.$destroy(); wideRestored.$destroy();
    wide.$destroy(); recoveredWide.$destroy();
    e.$destroy(); restored.$destroy();
    console.log('Creator: tools, flood-fill boundaries, undo/redo, frame operations, imports, draft recovery, storage failures and live play/pause passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });

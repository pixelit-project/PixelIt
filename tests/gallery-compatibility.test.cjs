const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const Vue = require('vue');
const helpers = require('./gallery-helpers.cjs');
const { galleryDeviceLimits, galleryMetadata, galleryCompatibility, buildGalleryCommand } = helpers;
const small = { frames: 28, pixels: 1792, width: 32, height: 8, bitmapPixels: 256, messageBytes: 12288 };
const big = { ...small, frames: 160, pixels: 10240, messageBytes: 65535 };
function bitmap(frames, width = 32, height = 8) {
    return Object.freeze({ id: frames, name: 'Test', animated: frames > 1, sizeX: String(width), sizeY: height,
        rgB565Array: Array(frames).fill(JSON.stringify(Array(width * height).fill(65535))).join(',') + ',' });
}
for (const [width, count] of [[8, 28], [16, 14], [32, 7]]) {
    assert(galleryCompatibility(bitmap(count, width), small).ok);
    assert(!galleryCompatibility(bitmap(count + 1, width), small).ok);
}
const large = bitmap(40);
assert.match(galleryCompatibility(large, small).reason, /40 frames, maximum 7/);
assert(galleryCompatibility(large, big).ok);
assert(!galleryCompatibility(bitmap(41), big).ok);
assert(galleryCompatibility(bitmap(160, 8), big).ok);
assert(!galleryCompatibility(bitmap(161, 8), big).ok);
assert(!galleryCompatibility(bitmap(1, 8, 16), small).ok);
assert(galleryCompatibility(bitmap(1), { ...small, pixels: 1 }).ok, 'Static full-screen images use bitmap budget');
assert.strictEqual(galleryMetadata(large), galleryMetadata(large), 'Retain metadata only, not parsed frame arrays');
assert.equal(galleryMetadata(large).frames, 40);
assert(!('data' in galleryMetadata(large)));
assert.equal(galleryDeviceLimits({ socket: { isConnected: false }, galleryCapabilities: big }), null);
assert.equal(galleryDeviceLimits({ socket: { isConnected: true }, galleryCapabilities: null }), null);
const legacy = galleryDeviceLimits({ socket: { isConnected: true }, galleryCapabilities: {} });
assert.equal(legacy.frames, 28); assert.equal(legacy.pixels, 1792);
assert.throws(() => buildGalleryCommand(large, small), /maximum 7/);
assert.equal(buildGalleryCommand(large, big).setScreen.bitmapAnimation.data.length, 40);
assert.equal(buildGalleryCommand(bitmap(1, 16, 4), big).setScreen.bitmap.size.height, 4);
assert.throws(() => buildGalleryCommand(bitmap(1, 8), { ...small, messageBytes: 20 }), /maximum 20 bytes/);
for (const source of ['[1,2]', '[1e400]', '[', JSON.stringify(Array(64).fill(65536)), JSON.stringify(Array(64).fill('0'))]) {
    assert.throws(() => buildGalleryCommand({ sizeX: 8, sizeY: 8, rgB565Array: source }, small));
}
const componentSource = fs.readFileSync(require('path').join(__dirname, '../src/views/Gallery.vue'), 'utf8')
    .split('<script>')[1].split('</script>')[0].replace(/^import .*;$/gm, '').replace('export default', 'this.component =');
const env = { ...helpers, actionFeedback: {}, notify() {}, BmpCanvas: {}, BmpDialog: {}, ButtonBMPReport: {}, GalleryViewportItem: {} };
vm.runInNewContext(componentSource, env);
const records = [bitmap(7), large, bitmap(1, 8, 16)];
const state = Vue.observable({ bmpsFromAPI: records, socket: { isConnected: true }, galleryCapabilities: small });
const gallery = new Vue({ ...env.component, beforeCreate() { this.$store = { state }; } });
let sent = 0;
gallery.$socket = { readyState: 1, sendObj() { sent++; } };
(async () => {
    assert.equal(gallery.getBMPs.length, 1);
    assert.equal(gallery.hiddenCount, 2);
    gallery.showIncompatible = true; await Vue.nextTick();
    assert.equal(gallery.getBMPs.length, 3);
    assert.throws(() => gallery.sendBitmap(large), /maximum 7/);
    assert.equal(sent, 0);
    state.galleryCapabilities = big; await Vue.nextTick();
    gallery.sendBitmap(large); assert.equal(sent, 1);
    state.galleryCapabilities = small; await Vue.nextTick();
    assert.throws(() => gallery.sendBitmap(large), /maximum 7/, 'Revalidate after reconnect to smaller device');
    state.socket.isConnected = false; state.galleryCapabilities = null; await Vue.nextTick();
    assert.equal(gallery.getBMPs.length, 3, 'Offline browsing must keep all entries');
    assert.throws(() => gallery.sendBitmap(records[0]), /No device connection/);
    state.socket.isConnected = true; await Vue.nextTick();
    assert.throws(() => gallery.sendBitmap(records[0]), /Connect a device/, 'Wait for fresh capability advertisement');
    assert.equal(sent, 1, 'Rejected/offline/stale actions must never send');
    gallery.$destroy();
    console.log('Gallery device filtering, actual frame counts, budgets, reconnects, wire limits and zero-send rejection passed');
})().catch(error => { console.error(error); process.exitCode = 1; });

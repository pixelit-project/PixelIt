const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const path = require('path');
const context = { DownloadStats: {}, ButtonCondition: {},
    setTimeout: callback => { context.timer = callback; return 1; }, clearTimeout() {},
    AbortController, fetch: async () => ({ ok: false, status: 403 }),
    FormData: class { append(kind, file) { this.kind = kind; this.file = file; } },
    XMLHttpRequest: class {
        constructor() { this.upload = {}; context.request = this; }
        open(method, url) { this.method = method; this.url = url; }
        send(data) { this.data = data; }
    },
};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../src/utils/update.js'), 'utf8').replace(/export /g, ''), context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../src/views/Update.vue'), 'utf8').split('<script>')[1].split('</script>')[0]
    .replace(/^import .*;$/gm, '').replace('export default', 'this.component ='), context);
function create() {
    const c = context.component;
    const instance = { ...c.data(), $store: { state: { version: '2.5.5', telemetryData: '{"buildSection":"ESP32_ulanzi"}', socket: { isConnected: true } } }, $pixelitHost: 'pixelit.local', $socket: { close() {} } };
    for (const [name, fn] of Object.entries(c.methods)) instance[name] = fn.bind(instance);
    for (const [name, fn] of Object.entries(c.computed)) Object.defineProperty(instance, name, { get: fn.bind(instance) });
    return instance;
}
const file = { name: 'firmware_v2.5.6_ESP32_ulanzi.bin', size: 100 };
(async () => {
    const e = create();
    e.latestRelease = { tag_name: '2.5.6', assets: [{ name: file.name }, { name: file.name.replace('.bin', '.full-upgrade.bin') }] };
    assert.equal(e.matchingAsset.name, file.name);
    e.$store.state.telemetryData = 'invalid'; assert.equal(e.hardware, ''); assert.equal(e.matchingAsset, null);
    e.$store.state.telemetryData = '{"buildSection":"ESP32_ulanzi"}';
    e.prepareUpdate('firmware', { ...file, name: 'firmware_v2.5.6_ESP32_generic.bin' });
    assert(e.fileWarning.includes('ESP32_generic')); e.startUpdate(); assert(!context.request, 'Mismatch requires acknowledgment');
    e.prepareUpdate('firmware', { ...file, name: file.name.replace('.bin', '.full-upgrade.bin') });
    e.acceptFileWarning = true; e.startUpdate(); assert(e.fileBlocked); assert(!context.request);
    e.prepareUpdate('filesystem', file); assert(e.fileBlocked);
    e.prepareUpdate('firmware', { ...file, size: 0 }); assert(e.fileBlocked);
    e.prepareUpdate('firmware', file); assert.equal(e.fileWarning, ''); e.startUpdate();
    let r = context.request;
    assert.equal(r.url, 'http://pixelit.local/update'); assert.equal(r.data.kind, 'firmware');
    r.upload.onprogress({ lengthComputable: true, loaded: 40, total: 100 }); assert.equal(e.uploadProgress, 40);
    r.upload.onload(); assert.equal(e.updateStatus, 'installing');
    r.status = 200; r.responseText = 'FAIL'; r.onload(); assert.equal(e.updateStatus, 'error');
    e.startUpdate(); r = context.request; r.onerror(); assert.equal(e.updateStatus, 'error'); assert(e.updateMessage.includes('unknown'));
    e.startUpdate(); r = context.request; r.ontimeout(); assert.equal(e.updateStatus, 'error');
    e.startUpdate(); r = context.request; r.status = 500; r.responseText = ''; r.onload(); assert.equal(e.updateStatus, 'error');
    e.startUpdate(); r = context.request; r.status = 200; r.responseText = 'OK'; r.onload(); assert.equal(e.updateStatus, 'restarting');
    context.component.watch.sockedIsConnected.call(e, true); assert.equal(e.updateStatus, 'restarting', 'Existing connection must not count as a restart');
    context.component.watch.sockedIsConnected.call(e, false);
    context.component.watch.sockedIsConnected.call(e, true); assert.equal(e.updateStatus, 'connected');
    e.startUpdate(); r = context.request; r.status = 200; r.responseText = 'OK'; r.onload(); context.timer(); assert.equal(e.updateStatus, 'error');
    await e.loadLatestRelease(); assert(e.releaseError, 'Release API errors must be visible');
    e.$demoMode = true; assert(!e.canUpdate);
    console.log('Update checks passed');
})().catch(error => { console.error(error); process.exitCode = 1; });

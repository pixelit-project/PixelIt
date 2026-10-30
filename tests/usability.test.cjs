const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const Vue = require('vue');
const notifications = [];
let confirmLeave = false;
let sent;
const context = {
    Vue, SyntaxError, notify: (message, color) => notifications.push({ message, color }),
    window: { confirm: () => confirmLeave, addEventListener() {}, removeEventListener() {} },
    setTimeout: () => 1, clearTimeout() {},
};
vm.createContext(context);
function load(relative, symbol) {
    let text = fs.readFileSync(path.join(__dirname, '..', relative), 'utf8');
    if (text.includes('<script>')) text = text.split('<script>')[1].split('</script>')[0];
    text = text.replace(/^import .*;$/gm, '').replace('export default', `this.${symbol} =`);
    vm.runInContext(text, context);
}
// Exercise the actual connection/error handling rather than a stubbed send function.
let feedbackSource = fs.readFileSync(path.join(__dirname, '../src/utils/feedback.js'), 'utf8');
feedbackSource = feedbackSource.replace(/^import .*;$/gm, '').replace(/export /g, '').replace(/function notify\([\s\S]*?\n}\n/, '');
vm.runInContext(feedbackSource, context);
load('src/mixins/configDraft.js', 'draftMixin');
load('src/mixins/actionFeedback.js', 'actionMixin');
load('src/components/Log.vue', 'logComponent');
load('src/components/ButtonBMPSave.vue', 'bitmapSaveComponent');
const store = Vue.observable({ state: { configData: { hostname: 'original', nested: { pin: 3 } }, socket: { isConnected: true } } });
function draft() {
    return new Vue({ ...context.draftMixin, beforeCreate() {
        this.$store = store;
        this.$socket = { readyState: 1, sendObj: data => { sent = data; } };
    }, data() { return { ...context.draftMixin.data(), isValid: true }; } });
}
(async () => {
    const e = draft();
    assert(e.configReady); assert(!e.hasUnsavedChanges);
    e.config.nested.pin = 4;
    assert(e.hasUnsavedChanges); assert.equal(store.state.configData.nested.pin, 3, 'Draft must not mutate the shared device configuration');
    store.state.configData.hostname = 'remote'; await Vue.nextTick();
    assert.equal(e.config.hostname, 'original', 'Incoming data must preserve a dirty draft');
    let prevented = false;
    const unload = { preventDefault() { prevented = true; } }; e.warnBeforeUnload(unload); assert(prevented);
    let routeResult;
    context.draftMixin.beforeRouteLeave.call(e, {}, {}, value => { routeResult = value; }); assert.equal(routeResult, false);
    confirmLeave = true;
    context.draftMixin.beforeRouteLeave.call(e, {}, {}, value => { routeResult = value; }); assert.equal(routeResult, undefined);
    e.resetDraft(); assert(!e.hasUnsavedChanges); assert.equal(e.config.hostname, 'remote');
    e.config.hostname = 'edited'; store.state.socket.isConnected = false; e.saveDraftConfig();
    assert(e.hasUnsavedChanges); assert.equal(sent, undefined); assert.equal(notifications.at(-1).color, 'error');
    store.state.socket.isConnected = true; e.saveDraftConfig();
    assert.equal(sent.setConfig.hostname, 'edited'); assert(!e.hasUnsavedChanges); assert.equal(store.state.configData.hostname, 'remote');
    assert(notifications.at(-1).message.includes('sent'), 'Do not claim persistence without a device acknowledgment');
    const log = new Vue({ ...context.logComponent, propsData: { log: 'INFO connected\nERROR upload' } });
    log.filter = 'error'; assert.equal(log.filteredLog, 'ERROR upload');
    log.togglePause(); log.log = 'INFO new message'; log.filter = ''; assert(log.filteredLog.includes('ERROR upload'));
    log.togglePause(); assert.equal(log.filteredLog, 'INFO new message');
    let calls = 0;
    const action = { $options: { feedbackActions: { send: 'Command sent' } }, $store: store, $socket: { readyState: 1 }, send() { calls++; throw new SyntaxError('bad JSON'); } };
    context.actionMixin.created.call(action);
    await action.send(); assert.equal(calls, 1); assert(notifications.at(-1).message.includes('Invalid JSON'));
    store.state.socket.isConnected = false; await action.send(); assert.equal(calls, 1);
    const upload = { saving: false, isValid: true, cleanedData: '[0]', userName: '', bitmapName: 'Test', getSizeX: 8, isAnimated: false, dialog: true, $apiServerBaseURL: 'https://example.test', $client: 'test' };
    context.fetch = async () => ({ ok: false, status: 500 });
    await context.bitmapSaveComponent.methods.agreeWraper.call(upload);
    assert(upload.dialog, 'A failed gallery upload must keep the dialog and drawing available');
    assert(!upload.saving); assert.equal(notifications.at(-1).color, 'error');
    context.fetch = async () => ({ ok: true });
    await context.bitmapSaveComponent.methods.agreeWraper.call(upload);
    assert(!upload.dialog); assert.equal(notifications.at(-1).color, 'success');
    e.$destroy(); log.$destroy();
    console.log('Usability: isolated drafts, leave/reload guards, offline saves, feedback, log filtering and pause passed');
})().catch(error => { console.error(error); process.exitCode = 1; });

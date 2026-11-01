const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const Vue = require('vue');
const compiler = require('vue-template-compiler');
const source = fs.readFileSync(path.join(__dirname, '../src/components/ButtonBMPReport.vue'), 'utf8');
// parseComponent retains nested slot templates, unlike simple string splitting.
assert.deepEqual(compiler.compile(compiler.parseComponent(source).template.content).errors, []);
let requests = [];
let notices = [];
let respond = async () => ({ status: 202 });
const context = {
    AbortController,
    fetch: async (url, options) => { requests.push({ url, options }); return respond(url, options); },
    notify: (message, type) => notices.push({ message, type }),
};
vm.runInNewContext(source.split('<script>')[1].split('</script>')[0]
    .replace(/^import .*;$/gm, '').replace('export default', 'this.component ='), context);
function create(id = 123) {
    const component = new Vue({ ...context.component, propsData: { bmp: { id, name: 'Test icon' } } });
    component.$apiServerBaseURL = 'https://api.example.test/api/';
    component.$client = 'PixelIt-Webui';
    component.$demoMode = false;
    return component;
}

(async () => {
    const dialog = create();
    dialog.reasonType = 'other';
    for (const reason of ['', '   ', null, 'x'.repeat(1001)]) {
        dialog.reason = reason;
        await dialog.submitReport();
    }
    assert.equal(requests.length, 0, 'Invalid reasons must not be sent');
    const invalid = create('invalid'); invalid.reasonType = 'duplicate'; invalid.reason = 'Duplicate'; await invalid.submitReport();
    assert.equal(requests.length, 0, 'Invalid IDs must not be sent'); invalid.$destroy();
    dialog.reason = 'Duplicate'; dialog.$demoMode = true; await dialog.submitReport();
    assert.equal(requests.length, 0, 'Demo mode must not submit real reports'); dialog.$demoMode = false;

    for (const type of ['duplicate', 'incorrect', 'inappropriate', 'own-upload']) {
        dialog.reasonType = type;
        dialog.reason = '';
        assert(dialog.canSubmit, 'Preset reasons allow empty optional details');
        assert(dialog.formattedReason.length > 0);
    }
    dialog.reasonType = null;
    dialog.reason = 'Some details';
    assert(!dialog.canSubmit, 'Require a selected reason');
    dialog.reasonType = 'unknown';
    assert(!dialog.canSubmit, 'Reject unknown preset values');
    dialog.reasonType = 'other';
    dialog.reason = 'x'.repeat(dialog.maximumReasonLength);
    assert(dialog.canSubmit);
    assert.equal(dialog.formattedReason.length, 1000, 'The prefix counts toward the API limit');
    dialog.reason += 'x';
    assert(!dialog.canSubmit);

    let finish;
    respond = () => new Promise(resolve => { finish = resolve; });
    dialog.dialog = true; await Vue.nextTick();
    dialog.reasonType = 'duplicate';
    dialog.reason = '  Same as #456  ';
    const pending = dialog.submitReport();
    await dialog.submitReport();
    assert.equal(requests.length, 1, 'Prevent duplicate submissions while saving');
    dialog.closeDialog(); assert(dialog.dialog, 'Keep dialog mounted while submission is pending');
    assert.equal(requests[0].url, 'https://api.example.test/api/reports');
    assert.deepEqual(JSON.parse(requests[0].options.body), { iconId: 123, reason: 'Duplicate icon: Same as #456' });
    assert.equal(requests[0].options.method, 'POST');
    finish({ status: 202 }); await pending;
    assert.equal(dialog.dialog, false); assert.equal(dialog.reason, '');
    assert.equal(notices[0].type, 'success');

    for (const status of [400, 404, 429, 500, 503]) {
        dialog.dialog = true; await Vue.nextTick(); dialog.reasonType = 'other'; dialog.reason = 'Keep this reason';
        respond = async () => ({ status });
        await dialog.submitReport();
        assert(dialog.dialog); assert.equal(dialog.reason, 'Keep this reason');
        assert(dialog.errorMessage); assert.equal(dialog.submitting, false);
    }
    assert.equal(notices.length, 1, 'Errors must not produce success notifications');
    respond = async () => { throw Error('Network unavailable'); };
    await dialog.submitReport(); assert.equal(dialog.reason, 'Keep this reason');

    const destroyed = create(); destroyed.reasonType = 'other'; destroyed.reason = 'Review this icon';
    let signal;
    respond = (url, options) => new Promise((resolve, reject) => {
        signal = options.signal;
        signal.addEventListener('abort', () => reject(Error('Aborted')));
    });
    const aborting = destroyed.submitReport();
    destroyed.$destroy(); await aborting;
    assert(signal.aborted, 'Navigation must cancel an in-flight request');
    assert.equal(notices.length, 1);
    dialog.$destroy();
    console.log('Icon reporting validation, persistence errors, duplicate suppression and cleanup passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });

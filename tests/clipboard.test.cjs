const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const source = fs.readFileSync(path.join(__dirname, '../src/utils/clipboard.js'), 'utf8')
    .replace('export async function copyToClipboard', 'this.copyToClipboard = async function');

function setup(clipboard, copyResult = true) {
    const state = { appended: 0, removed: 0, restored: 0, copied: 0 };
    const input = {
        style: {}, setAttribute() {}, focus() {}, select() {},
        setSelectionRange(start, end) { state.selection = [start, end]; },
    };
    const context = {
        navigator: { clipboard },
        document: {
            activeElement: { focus() { state.restored++; } },
            createElement() { return input; },
            body: { appendChild() { state.appended++; }, removeChild() { state.removed++; } },
            execCommand(command) {
                assert.equal(command, 'copy');
                state.copied++;
                if (copyResult instanceof Error) throw copyResult;
                return copyResult;
            },
        },
    };
    vm.runInNewContext(source, context);
    return { state, input, copy: context.copyToClipboard };
}

(async () => {
    let copied;
    const modern = setup({ writeText: async text => { copied = text; } });
    await modern.copy(1234);
    assert.equal(copied, '1234');
    assert.equal(modern.state.appended, 0);

    for (const clipboard of [undefined, { writeText: async () => { throw Error('Permission denied'); } }]) {
        const fallback = setup(clipboard);
        await fallback.copy(5678);
        assert.equal(fallback.input.value, '5678');
        assert.deepEqual(fallback.state.selection, [0, 4]);
        assert.equal(fallback.state.copied, 1);
        assert.equal(fallback.state.removed, 1);
        assert.equal(fallback.state.restored, 1);
    }
    for (const result of [false, Error('Copy blocked')]) {
        const blocked = setup(undefined, result);
        await assert.rejects(blocked.copy('123'));
        assert.equal(blocked.state.removed, 1);
        assert.equal(blocked.state.restored, 1);
    }
    console.log('Clipboard tests passed: modern API, HTTP fallback, denied permissions and cleanup.');
})().catch(error => { console.error(error); process.exitCode = 1; });

<template>
    <div class="Log">
        <div class="log-tools">
            <v-text-field v-model="filter" label="Filter logs" prepend-inner-icon="mdi-magnify" dense outlined hide-details clearable />
            <v-btn small outlined :aria-pressed="String(paused)" @click="togglePause"><v-icon left small>{{ paused ? 'mdi-play' : 'mdi-pause' }}</v-icon>{{ paused ? 'Resume' : 'Pause' }}</v-btn>
        </div>
        <p v-if="paused" class="text-caption mt-2 mb-0" role="status">Paused view. New messages are still collected.</p>
        <div class="log mt-4" role="region" aria-label="Device logs" tabindex="0">
            <template v-if="filteredLog"><span v-for="(line, index) in highlightedLines" :key="index" class="log-line" :class="line.level"><span class="log-time">{{ line.time }}</span><span class="log-function">{{ line.source }}</span><span><span v-for="(token, tokenIndex) in line.tokens" :key="tokenIndex" :class="token.type">{{ token.text }}</span></span>{{ highlightedLines.length - 1 > index ? '\n' : '' }}</span></template>
            <span v-else class="log-placeholder">{{ filter ? 'No messages match your filter.' : 'Waiting for log messages…' }}</span>
        </div>
    </div>
</template>

<script>
export default {
    name: 'Log',
    props: { log: { type: String, required: true } },
    data: () => ({ filter: '', paused: false, snapshot: '' }),
    computed: {
        filteredLog() {
            const text = this.paused ? this.snapshot : this.log;
            const query = (this.filter || '').toLowerCase();
            const filtered = query ? text.split('\n').filter(line => line.toLowerCase().includes(query)).join('\n') : text;
            return filtered.replace(/^\[\d{4}-\d{2}-\d{2}T(\d{2}:\d{2}:\d{2})(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?\]/gm, '[$1]');
        },
        highlightedLines() {
            return this.filteredLog.split('\n').map(line => {
                const match = line.match(/^(\[\d{2}:\d{2}:\d{2}\] )(?:([^:\n]+: ))?(.*)$/);
                const message = match ? match[3] : line;
                const level = /\b(error|failed|failure|fatal)\b/i.test(message) ? 'log-error'
                    : /\b(warn|warning)\b/i.test(message) ? 'log-warning' : '';
                return { time: match ? match[1] : '', source: match ? match[2] || '' : '', message, level, tokens: this.tokenizeMessage(message) };
            });
        },
    },
    methods: {
        tokenizeMessage(message) {
            // Type names are values in the firmware's list, not a fixed vocabulary.
            const contents = message.match(/^(JSON contains\s+)([^():\n]+?)(?=,\s*[A-Za-z_][\w-]*:\s|\s*\(|$)/);
            if (contents) {
                const list = contents[2].split(/(,\s*)/).map((text, index) => ({
                    text, type: index % 2 ? '' : 'log-literal',
                }));
                return [{ text: contents[1], type: '' }, ...list,
                    ...this.tokenizeMessage(message.slice(contents[0].length))];
            }
            // Preserve every character; Vue renders tokens as text, never HTML.
            const pattern = /"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|\b[A-Za-z_][\w-]*(?=:\s)|\b(?:true|false|null)\b|\b(?:0x[\da-f]+|\d+(?:\.\d+)*)\b/gi;
            const tokens = [];
            let offset = 0;
            let match;
            while ((match = pattern.exec(message))) {
                if (match.index > offset) tokens.push({ text: message.slice(offset, match.index), type: '' });
                const text = match[0];
                const type = /^["']/.test(text) ? 'log-string' : /^(true|false|null)$/i.test(text) ? 'log-literal'
                    : /^\d/.test(text) ? 'log-number' : 'log-key';
                tokens.push({ text, type });
                offset = pattern.lastIndex;
            }
            if (offset < message.length) tokens.push({ text: message.slice(offset), type: '' });
            return tokens;
        },
        togglePause() { if (!this.paused) this.snapshot = this.log; this.paused = !this.paused; },
    },
};
</script>

<style scoped>
.log-tools { display: flex; align-items: center; flex-wrap: wrap; gap: 12px; }
.log-tools .v-input { flex: 1; min-width: 140px; }
.log { font-family: Consolas, monospace; font-size: 12px; letter-spacing: -0.3px; line-height: 16px; height: 380px; min-height: 160px; overflow: auto; resize: vertical; padding: 12px; border: 1px solid rgba(128, 128, 128, .5); border-radius: 4px; background: rgba(128, 128, 128, .08); white-space: pre; user-select: text; }
.log:focus { outline: 2px solid var(--v-primary-base, #1976d2); outline-offset: 1px; }
.log-time, .log-placeholder { color: #616161; }
.log-function { color: #006b83; }
.log-error { color: #b71c1c; }
.log-warning { color: #885000; }
.theme--dark .log-time, .theme--dark .log-placeholder { color: #aeb6c2; }
.theme--dark .log-function { color: #80cbc4; }
.theme--dark .log-error { color: #ff8a80; }
.theme--dark .log-warning { color: #ffd180; }
.log-string { color: #33691e; }
.log-number { color: #805200; }
.log-literal { color: #6a1b9a; }
.log-key { color: #1558a6; }
.theme--dark .log-string { color: #c5e1a5; }
.theme--dark .log-number { color: #ffcc80; }
.theme--dark .log-literal { color: #ce93d8; }
.theme--dark .log-key { color: #90caf9; }
.log-error span, .log-warning span { color: inherit; }
</style>

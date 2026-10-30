<template>
    <div class="Log">
        <div class="log-tools">
            <v-text-field v-model="filter" label="Filter logs" prepend-inner-icon="mdi-magnify" dense outlined hide-details clearable />
            <v-btn small outlined :aria-pressed="String(paused)" @click="togglePause"><v-icon left small>{{ paused ? 'mdi-play' : 'mdi-pause' }}</v-icon>{{ paused ? 'Resume' : 'Pause' }}</v-btn>
        </div>
        <p v-if="paused" class="text-caption mt-2 mb-0" role="status">Paused view. New messages are still collected.</p>
        <v-textarea :value="filteredLog" label="Device logs" filled outlined readonly class="log mt-4" rows="14" hide-details :placeholder="filter ? 'No messages match your filter.' : 'Waiting for log messages…'" />
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
            return query ? text.split('\n').filter(line => line.toLowerCase().includes(query)).join('\n') : text;
        },
    },
    methods: {
        togglePause() { if (!this.paused) this.snapshot = this.log; this.paused = !this.paused; },
    },
};
</script>

<style scoped>
.log-tools { display: flex; align-items: center; flex-wrap: wrap; gap: 12px; }
.log-tools .v-input { flex: 1; min-width: 140px; }
.log { font-family: Consolas, monospace; font-size: 12px; line-height: 1.5; }
</style>

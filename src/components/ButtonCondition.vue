<template>
    <v-btn :color="color" elevation="2" @click="runAction" :loading="busy" :disabled="!condition || busy">
        <v-icon left>{{ icon }}</v-icon>
        {{ text }}
    </v-btn>
</template>

<script>
import { notify } from '../utils/feedback';
export default {
    data: () => ({ busy: false }),
    methods: {
        async runAction() {
            if (!this.condition || this.busy || !this.onclick) return;
            this.busy = true;
            try { await this.onclick(); }
            catch (error) { notify(error.message || 'Action failed. Please try again.', 'error'); }
            finally { this.busy = false; }
        },
    },
    props: {
        condition: {
            type: Boolean,
            required: true,
        },
        text: {
            type: String,
            required: true,
        },
        color: {
            type: String,
            required: true,
        },
        onclick: {
            type: Function,
            required: false,
        },
        icon: {
            type: String,
            required: true,
        },
    },
};
</script>

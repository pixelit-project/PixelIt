import { sendCommand } from '../utils/feedback';

const clone = value => JSON.parse(JSON.stringify(value || {}));
export default {
    data: () => ({ config: {}, savedConfig: '{}', configReady: false }),
    computed: {
        hasUnsavedChanges() { return this.configReady && JSON.stringify(this.config) !== this.savedConfig; },
    },
    watch: {
        '$store.state.configData': {
            immediate: true,
            deep: true,
            handler(value) {
                if (!this.hasUnsavedChanges && Object.keys(value || {}).length) this.resetDraft(value);
            },
        },
    },
    mounted() { window.addEventListener('beforeunload', this.warnBeforeUnload); },
    beforeDestroy() { window.removeEventListener('beforeunload', this.warnBeforeUnload); clearTimeout(this.configRestartTimer); },
    beforeRouteLeave(to, from, next) {
        if (!this.hasUnsavedChanges || window.confirm('Discard unsaved configuration changes and leave this page?')) next();
        else next(false);
    },
    methods: {
        resetDraft(value = this.$store.state.configData) {
            this.config = clone(value);
            this.savedConfig = JSON.stringify(this.config);
            this.configReady = true;
        },
        warnBeforeUnload(event) {
            if (!this.hasUnsavedChanges) return;
            event.preventDefault();
            event.returnValue = '';
        },
        saveDraftConfig() {
            if (!this.isValid || !this.configReady || !this.hasUnsavedChanges) return;
            if (sendCommand(this, { setConfig: clone(this.config) }, 'Configuration sent. PixelIt will restart; reconnect to verify your settings.')) {
                this.savedConfig = JSON.stringify(this.config);
                this.configRestartTimer = setTimeout(() => { if (this.$socket) this.$socket.close(); }, 3000);
            }
        },
    },
};

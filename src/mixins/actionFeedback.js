import { notify } from '../utils/feedback';

// Wrap explicit user actions, never automatic live-draw or telemetry updates.
export default {
    created() {
        for (const [name, message] of Object.entries(this.$options.feedbackActions || {})) {
            const action = this[name];
            this[name] = async (...args) => {
                try {
                    if (this.$demoMode || !this.$store.state.socket.isConnected || !this.$socket || this.$socket.readyState !== 1) {
                        throw new Error('No device connection. Reconnect your PixelIt and try again.');
                    }
                    await action(...args);
                    if (message) notify(message, 'success');
                } catch (error) {
                    notify(error instanceof SyntaxError ? 'Invalid JSON data. Check the input and try again.' : error.message || 'Action failed. Please try again.', 'error');
                }
            };
        }
    },
};

import Vue from 'vue';

export const feedback = Vue.observable({ visible: false, message: '', color: 'info' });
export function notify(message, color = 'info') {
    feedback.visible = false;
    feedback.message = message;
    feedback.color = color;
    Vue.nextTick(() => { feedback.visible = true; });
}

export function sendCommand(vm, command, message) {
    try {
        if (vm.$demoMode) throw new Error('Demo mode: changes cannot be sent to a device.');
        if (!vm.$store.state.socket.isConnected || !vm.$socket || vm.$socket.readyState !== 1) {
            throw new Error('PixelIt is offline. Reconnect and try again.');
        }
        vm.$socket.sendObj(command);
        if (message) notify(message, 'success');
        return true;
    } catch (error) {
        notify(error.message || 'The command could not be sent. Please try again.', 'error');
        return false;
    }
}

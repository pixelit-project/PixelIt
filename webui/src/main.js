import '@babel/polyfill';
import 'mutationobserver-shim';
import Vue from 'vue';
import App from './App.vue';
import router from './router';
import store from './store';
import vuetify from './plugins/vuetify';
import VueCookies from 'vue-cookies';
import VueNativeSock from 'vue-native-websocket';
import VueSpinners from 'vue-spinners';
import 'leaflet/dist/leaflet.css';
import demoJSON from '../public/demoData/demo.json';

if (process.env.VUE_APP_PIXELIT_HOST !== undefined) {
    Vue.prototype.$pixelitHost = process.env.VUE_APP_PIXELIT_HOST;
} else {
    Vue.prototype.$pixelitHost = location.host;
}

Vue.use(VueSpinners);
Vue.use(VueCookies);

// Demo mode
if (location.host.includes('.github.io') || (process.env.VUE_APP_DEMO_MODE !== undefined && process.env.VUE_APP_DEMO_MODE == 'true')) {
    store.commit('SOCKET_ONMESSAGE', demoJSON);
    Vue.prototype.$demoMode = true;
}
// Prod mode
else {
    Vue.use(VueNativeSock, `ws://${location.host}:81`, {
        store: store,
        reconnection: true,
        format: 'json',
    });
    Vue.prototype.$demoMode = false;
}

Vue.$cookies.config('10y');
Vue.config.productionTip = false;
new Vue({
    router,
    store,
    vuetify,
    render: (h) => h(App),
}).$mount('#app');

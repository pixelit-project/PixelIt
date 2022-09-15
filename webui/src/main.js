import '@babel/polyfill'
import 'mutationobserver-shim'
import Vue from 'vue'
import App from './App.vue'
import router from './router'
import store from './store'
import vuetify from './plugins/vuetify'
import VueCookies from 'vue-cookies'
import VueNativeSock from 'vue-native-websocket'
import VueSpinners from 'vue-spinners'
import 'leaflet/dist/leaflet.css'
import demoJSON from '../public/demoData/demo.json'

let url;
if (process.env.VUE_APP_PIXELIT_HOST !== undefined) {
  url = process.env.VUE_APP_PIXELIT_HOST;
} else {
  url = location.host;
}

Vue.use(VueSpinners);
Vue.use(VueCookies);

// Demo mode
if (location.host.includes('.github.io')) {
  store.commit('SOCKET_ONMESSAGE', demoJSON);
}
// Prod mode
else {
  Vue.use(VueNativeSock, `ws://${url}:81`, {
    store: store,
    reconnection: true,
    format: 'json'
  });
}

Vue.$cookies.config('10y');
Vue.config.productionTip = false;
new Vue({
  router,
  store,
  vuetify,
  render: h => h(App)
}).$mount('#app')
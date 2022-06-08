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


let url;
if (location.host.includes(":")) {
  url = "192.168.3.5";
} else {
  url = location.host;
}

Vue.use(VueSpinners);
Vue.use(VueCookies);
Vue.use(VueNativeSock, `ws://${url}:81`, {
  store: store,
  reconnection: true,
  format: 'json'
});
Vue.$cookies.config('10y');
Vue.config.productionTip = false;
new Vue({
  router,
  store,
  vuetify,
  render: h => h(App)
}).$mount('#app')
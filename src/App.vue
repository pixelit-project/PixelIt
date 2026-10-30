<template>
    <v-app>
        <!-- Sidebar -->
        <v-navigation-drawer v-model="drawer" class="app-navigation" app>
            <v-img class="mt-3" max-height="100" src="./assets/logo.png" contain></v-img>
            <nav-links :items="getNavLinks" />
        </v-navigation-drawer>
        <!-- Bar -->
        <v-app-bar app>
            <v-app-bar-nav-icon aria-label="Toggle navigation" @click="drawer = !drawer"></v-app-bar-nav-icon>
            <v-toolbar-title><b>PixelIt</b><span class="brand-subtitle"> - The Matrix Display</span></v-toolbar-title>
            <v-spacer />
            <div v-if="displayHostname" class="hostname padded" :title="`Hostname: ${displayHostname}`">{{ displayHostname }}</div>
            <v-chip small outlined class="connection-status mr-2" :color="isDemoMode ? 'info' : sockedIsConnected ? 'success' : connectionFailed ? 'error' : 'warning'" role="status" aria-live="polite">
                <v-icon left small>{{ sockedIsConnected || isDemoMode ? 'mdi-lan-connect' : 'mdi-lan-disconnect' }}</v-icon>
                {{ isDemoMode ? 'Demo' : sockedIsConnected ? 'Connected' : connectionFailed ? 'Offline' : 'Connecting…' }}
            </v-chip>
            <v-btn icon @click="changeTheme" title="Change theme" aria-label="Change theme">
                <v-icon v-if="darkModeActive">mdi-brightness-4</v-icon>
                <v-icon v-else>mdi-brightness-4</v-icon>
            </v-btn>
        </v-app-bar>
        <!-- Content -->
        <v-main app>
            <router-view />
        </v-main>
        <v-snackbar v-model="feedback.visible" :color="feedback.color" :timeout="feedback.color === 'error' ? 10000 : 5000" bottom>
            <span role="status" aria-live="polite">{{ feedback.message }}</span>
            <template v-slot:action="{ attrs }"><v-btn icon v-bind="attrs" aria-label="Dismiss message" @click="feedback.visible = false"><v-icon>mdi-close</v-icon></v-btn></template>
        </v-snackbar>
    </v-app>
</template>

<script>
import NavLinks from './components/NavLinks';
import { feedback } from './utils/feedback';
import './styles/ui.css';
export default {
    created: function () {
        // Get style cookie
        this.$vuetify.theme.dark = this.$cookies.get('theme_dark') ? this.$cookies.get('theme_dark') === 'true' : true;

        this.trackConnection(this.sockedIsConnected);
        getCurrentGitReleaseData(this);    
        sendTelemetry(this);
        getStatistics(this);

        // Check again every 15 minutes
        setInterval(() => {
            getCurrentGitReleaseData(this);
        }, 1000 * 60 * 15);

        // Check again every 15 minutes
        setInterval(() => {
            getStatistics(this);
        }, 1000 * 60 * 15);

        // Send again every 12 houres
        setInterval(() => {
            sendTelemetry(this);
        }, 1000 * 60 * 60 * 12);
    },
    components: {
        NavLinks,
    },
    data: () => ({
        drawer: null,
        feedback,
        connectionFailed: false,
        connectionTimer: null,
    }),
    computed: {
        darkModeActive() {
            return this.$vuetify.theme.dark;
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        isDemoMode() {
            return this.$demoMode;
        },
        getNavLinks() {
            return this.$store.state.navLinks;
        },
        displayHostname() {
            return this.$store.state.displayHostname;
        },
    },
    beforeDestroy() { clearTimeout(this.connectionTimer); },
    methods: {
        trackConnection(connected) {
            clearTimeout(this.connectionTimer);
            this.connectionFailed = false;
            if (!connected && !this.isDemoMode) this.connectionTimer = setTimeout(() => { this.connectionFailed = true; }, 15000);
        },
        changeTheme() {
            this.$vuetify.theme.dark = !this.$vuetify.theme.dark;
            this.$cookies.set('theme_dark', this.$vuetify.theme.dark);
        },
    },
    watch: {
        sockedIsConnected: 'trackConnection',
        '$store.state.gitVersion': function (newVal) {
            if (this.$store.state.version) {
                //this.$store.state.newVersionAvailable = newVal != this.$store.state.version;
                this.$store.state.newVersionAvailable = isNewVersionAvailable(this.$store.state.version, newVal);
            }
        },
        '$store.state.version': function (newVal) {
            if (this.$store.state.gitVersion) {
                //this.$store.state.newVersionAvailable = newVal != this.$store.state.gitVersion;
                this.$store.state.newVersionAvailable = isNewVersionAvailable(newVal, this.$store.state.gitVersion);
            }
        },
    },
};

async function getCurrentGitReleaseData(vue) {
    try {
        vue.$store.state.gitReleases = await (await fetch(`${vue.$apiServerBaseURL}/releases`)).json();
        vue.$store.state.gitVersion = vue.$store.state.gitReleases[0].version;
        vue.$store.state.gitDownloadUrl = vue.$store.state.gitReleases[0].downloadURL;
    } catch (error) {
        console.log(`getCurrentGitReleaseData: error (${error})`);
    }
}

async function sendTelemetry(vue) {
    setTimeout(() => {
        if (vue.$store.state.configData != {} && vue.$store.state.telemetryData != '') {
            // if sendTelemetry disabled force return!
            if (vue.$store.state.configData.sendTelemetry == false) {
                return;
            }
            // send telemetry
            fetch(`${vue.$apiServerBaseURL}/telemetry`, {
                method: 'POST',      
                headers: { Accept: 'application/json', 'Content-Type': 'application/json', 'Client': vue.$client},      
                body: vue.$store.state.telemetryData,
            });
        } else {
            sendTelemetry(vue);
        }
    }, 1000);
}

async function getStatistics(vue) {
    const uuid = getUUID(vue);
    if (uuid == '') {
        setTimeout(() => {getStatistics(vue)}, 600)
        return;
    }

    try {
        vue.$store.state.userMapData = await (await fetch(`${vue.$apiServerBaseURL}/usermap?uuid=${uuid}`)).json();
    } catch (error) {
        console.log(`getUserMapData: error (${error})`);
    }
   
    try {      
        const response = await fetch(`${vue.$apiServerBaseURL}/statistics?uuid=${uuid}`);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        vue.$store.state.statistics = await response.json();
    } catch (error) {
        vue.$store.state.statistics = { error: true };
        console.log(`getStatistics: error (${error})`);
    }
}


function isNewVersionAvailable(local, git){
    return compareVersions(local, git) == -1
}

function getUUID(vue){
    if (vue.$route.query.uuid){
        return vue.$route.query.uuid;
    }

    if (vue.$store.state.telemetryData == '') {
        return '';
    }
    return JSON.parse(vue.$store.state.telemetryData).uuid
}

// 0 if a = b 
// 1 if a > b 
// -1 if a < b
const compareVersions = ((prep, l, i, r) => (a, b) =>{
    a = prep(a);
    b = prep(b);
    l = Math.max(a.length, b.length);
    i = 0;
    r = i;
    while (!r && i < l)
        //convert into integer, uncluding undefined values
      r = ~~a[i] - ~~b[i++];

    return r < 0 ? -1 : (r ? 1 : 0);
})(t => ("" + t)
    // treat non-numerical characters as lower version
    // replacing them with a negative number based on charcode of first character
  .replace(/[^\d.]+/g, c => "." + (c.replace(/[\W_]+/, "").toUpperCase().charCodeAt(0) - 65536) + ".")
    // remove trailing "." and "0" if followed by non-numerical characters (1.0.0b);
  .replace(/(?:\.0+)*(\.-\d+(?:\.\d+)?)\.*$/g, "$1")
    // return array
  .split("."));

</script>

<style>
::-webkit-scrollbar {
    width: 20px;
}

/* Track */
::-webkit-scrollbar-track {
    background-color: transparent;
}

/* Handle */
::-webkit-scrollbar-thumb {
    background-color: #8c8c8c;
    border-radius: 20px;
    border: 6px solid transparent;
    background-clip: content-box;
}

/* Handle on hover */
::-webkit-scrollbar-thumb:hover {
    background-color: #a8a8a8;
}
</style>
<style scoped>
.message {
    color: orange;
    font-size: 20px;
}

.hostname {
    font-size: 14px;
    text-transform: uppercase;
    padding-right: 15px;
}

@media screen and (max-width: 600px) {
    .hostname {
        display: none;
    }
}
</style>

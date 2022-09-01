<template>
    <v-app>
        <!-- Sidebar -->
        <v-navigation-drawer v-model="drawer" app>
            <v-img max-height="100" src="./assets/logo.png" contain></v-img>
            <nav-links :items="getNavLinks" />
        </v-navigation-drawer>
        <!-- Bar -->
        <v-app-bar app>
            <v-app-bar-nav-icon @click="drawer = !drawer"></v-app-bar-nav-icon>
            <v-toolbar-title> <b>PixelIt</b> the Matrix Display </v-toolbar-title>
            <v-spacer><p v-if="!sockedIsConnected" class="text-center message">Reconnecting... please wait</p></v-spacer>
            <v-icon v-if="sockedIsConnected" color="green" title="Connected">mdi-lan-connect</v-icon>
            <v-icon v-else color="red" title="Disconnected">mdi-lan-disconnect</v-icon>
            <v-btn icon @click="changeTheme" title="Change theme">
                <v-icon v-if="darkModeActive">mdi-brightness-5</v-icon>
                <v-icon v-else>mdi-brightness-7</v-icon>
            </v-btn>
        </v-app-bar>
        <!-- Content -->
        <v-main app>
            <router-view />
        </v-main>
    </v-app>
</template>

<script>
import NavLinks from './components/NavLinks';
export default {
    created: function () {
        // Get style cookie
        this.$vuetify.theme.dark = this.$cookies.get('theme_dark') ? this.$cookies.get('theme_dark') === 'true' : true;
        getCurrentGitReleaseData(this.$store.state);
        // Check again every 15 minutes
        setInterval(() => {
            getCurrentGitReleaseData(this.$store.state);
        }, 1000 * 60 * 15);

        getUserMapData(this.$store.state);

        // Check again every 15 minutes
        setInterval(() => {
            getUserMapData(this.$store.state);
        }, 1000 * 60 * 15);

        // sendTelemetry aktive?
        if (this.$store.state.configData || this.$store.state.configData.sendTelemetry) {
            sendTelemetry(this.$store.state);
            // Send again every 12 houres
            setInterval(() => {
                sendTelemetry(this.$store.state);
            }, 1000 * 60 * 60 * 12);
        }
    },
    components: {
        NavLinks,
    },
    data: () => ({
        drawer: true,
    }),
    computed: {
        darkModeActive() {
            return this.$vuetify.theme.dark;
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        getNavLinks() {
            return this.$store.state.navLinks;
        },
    },
    methods: {
        changeTheme() {
            this.$vuetify.theme.dark = !this.$vuetify.theme.dark;
            this.$cookies.set('theme_dark', this.$vuetify.theme.dark);
        },
    },
    watch: {
        '$store.state.gitVersion': function (newVal) {
            if (this.$store.state.version) {
                this.$store.state.newVersionAvailable = newVal != this.$store.state.version;
            }
        },
        '$store.state.version': function (newVal) {
            if (this.$store.state.gitVersion) {
                this.$store.state.newVersionAvailable = newVal != this.$store.state.gitVersion;
            }
        },
    },
};

async function getCurrentGitReleaseData(state) {
    try {
        state.gitReleases = await (await fetch('https://pixelit.bastelbunker.de/api/releases')).json();
        state.gitVersion = state.gitReleases[0].version;
        state.gitDownloadUrl = state.gitReleases[0].downloadURL;
    } catch (error) {
        console.log(`getCurrentGitReleaseData: error (${error})`);
    }
}

async function sendTelemetry(state) {
    setTimeout(() => {
        if (state.telemetryData != '') {
            fetch('https://pixelit.bastelbunker.de/api/telemetry', { method: 'POST', headers: { Accept: 'application/json', 'Content-Type': 'application/json' }, body: state.telemetryData });
        } else {
            sendTelemetry(state);
        }
    }, 1000);
}

async function getUserMapData(state) {
    try {
        state.userMapData = await (await fetch('https://pixelit.bastelbunker.de/api/usermapdata')).json();
    } catch (error) {
        console.log(`getUserMapData: error (${error})`);
    }
}
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
.container {
    max-width: 100vw;
    padding: 10px;
}

.message {
    color: orange;
}
</style>

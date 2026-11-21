<template>
    <v-container class="home app-page">
        <v-row>
            <v-col cols="12" lg="3">
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>System</h2>
                    </v-card-title>

                    <router-link v-if="newVersionAvailable && !isDemoMode" class="update-notice" :to="{ name: 'Update' }">
                        <v-icon class="update-notice__icon" size="20" aria-hidden="true">mdi-update</v-icon>
                        <span class="update-notice__text">
                            <strong>Update available</strong>
                            <span>Version {{ gitVersion }}</span>
                        </span>
                        <v-icon class="update-notice__arrow" size="18" aria-hidden="true">mdi-chevron-right</v-icon>
                    </router-link>
                    <v-skeleton-loader v-if="!systemItems.length" type="list-item-two-line@4" />
                    <ListInfo v-else :items="primarySystemItems" />
                    <v-expansion-panels v-if="technicalSystemItems.length" flat class="mt-4">
                        <v-expansion-panel>
                            <v-expansion-panel-header>Technical details</v-expansion-panel-header>
                            <v-expansion-panel-content><ListInfo :items="technicalSystemItems" /></v-expansion-panel-content>
                        </v-expansion-panel>
                    </v-expansion-panels>
                </v-card>
            </v-col>
            <v-col cols="12" lg="3">
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Liveview</h2>
                    </v-card-title>

                    <Liveview class="text-center" :data="liveview" :options="liveviewCanvasSettings" />
                </v-card>
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Sensors</h2>
                    </v-card-title>

                    <ListInfo v-if="sensorItems.length" :items="sensorItems" /><p v-else class="text--secondary mb-0">Waiting for sensor data…</p>
                </v-card>
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Buttons</h2>
                    </v-card-title>

                    <ListInfo v-if="buttonItems.length" :items="buttonItems" /><p v-else class="text--secondary mb-0">No button data available.</p>
                </v-card>
            </v-col>
            <v-col cols="12" lg="6">
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Logs</h2>
                    </v-card-title>

                    <Log :log="log" />
                </v-card>
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Usermap</h2>
                    </v-card-title>
                    <v-card-text class="text-md-center" v-if="userMapError || sendStatistics == false">
                        <h2>Activate the telemetry data to see the usermap.</h2>
                        <br>
                        <h4>To activate the telemetry data, go to <a href="/#/options"><b>Options</b> and activate <b>"Send Telemetry data"</b></a></h4>
                        <small>After activation, it can take up to one minute for the data to become visible.</small>
                    </v-card-text>
                    <div v-else>
                        <p></p>                    
                        <UserMap :coords="userMapData" height="500px" />
                    </div>
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
import Log from '../components/Log';
import ListInfo from '../components/ListInfo';
import UserMap from '../components/UserMap';
import Liveview from '../components/Liveview';

export default {
    name: 'Home',
    components: {
        Log,
        ListInfo,
        UserMap,
        Liveview,
    },
    computed: {
        primarySystemItems() {
            const names = ['PixelIt Version', 'Hostname', 'Note', 'IP-Address', 'Wifi quality', 'Uptime', 'Current Brightness'];
            return this.systemItems.filter(item => names.includes(item.name));
        },
        technicalSystemItems() {
            const names = ['Device-ID', 'ChipID', 'CPU freq.', 'Build Section', 'Wifi SSID', 'Wifi BSSID', 'Wifi RSSI', 'Sketch size', 'Free Sketch Space', 'Free heap', 'Reset reason'];
            return names.map(name => this.systemItems.find(item => item.name === name)).filter(Boolean);
        },
        systemItems() {
            return this.$store.state.sysInfoData;
        },
        sensorItems() {
            return this.$store.state.sensorData;
        },
        buttonItems() {
            return this.$store.state.buttonData;
        },
        log() {
            return this.$store.state.logData.join('\n');
        },
        newVersionAvailable() {
            return this.$store.state.newVersionAvailable;
        },
        gitVersion() {
            return this.$store.state.gitVersion;
        },
        userMapData() {
            return this.$store.state.userMapData.coords;
        },
        userMapError() {            
            return this.$store.state.userMapData.error;             
        },  
        isDemoMode() {
            return this.$demoMode;
        },
        liveview() {
            return this.$store.state.liveviewData;
        },
        liveviewCanvasSettings() {
            return this.$store.state.matrixSize;
        },
        sendStatistics(){
            return this.$store.state.configData.sendTelemetry;
        },
    },
};
</script>
<style scoped>
.update-notice {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 8px;
    padding: 10px;
    border: 1px solid rgba(195, 109, 43, 0.4);
    border-radius: 8px;
    background-color: rgba(195, 109, 43, 0.09);
    color: inherit;
    text-decoration: none;
}

.update-notice:hover,
.update-notice:focus-visible {
    background-color: rgba(195, 109, 43, 0.18);
}

.update-notice:focus-visible {
    outline: 2px solid currentColor;
    outline-offset: 2px;
}

.update-notice .update-notice__icon {
    flex: none;
    padding: 6px;
    border-radius: 6px;
    background-color: rgba(195, 109, 43, 0.15);
    color: #984305;
}

.theme--dark .update-notice .update-notice__icon {
    color: #ffb476;
}

.update-notice__text {
    flex: 1;
    min-width: 0;
    font-size: 14px;
    line-height: 1.35;
    overflow-wrap: anywhere;
}

.update-notice__text span {
    display: block;
    margin-top: 2px;
    font-size: 12px;
}

.update-notice .update-notice__arrow {
    flex: none;
    color: inherit;
}
</style>

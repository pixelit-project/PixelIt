<template>
    <v-container class="home">
        <v-row>
            <v-col cols="12" lg="3">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>System</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <div class="text-center updateMessage" v-if="newVersionAvailable && !isDemoMode">
                        <a class="updateMessage" :href="gitUpdateURL" target="_blank">Update to version {{ gitVersion }} available!</a>
                        <v-icon size="16px" class="updateMessage">mdi-open-in-new</v-icon>
                    </div>
                    <ListInfo :items="systemItems" />
                </v-card>
            </v-col>
            <v-col cols="12" lg="3">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Sensors</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <ListInfo :items="sensorItems" />
                </v-card>
                <br />
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Buttons</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <ListInfo :items="buttonItems" />
                </v-card>
            </v-col>
            <v-col cols="12" lg="6">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Logs</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <Log :log="log" />
                </v-card>
                <br />
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Usermap</h2>
                    </v-card-title>
                    <hr />
                    <p></p>
                    <UserMap :coords="userMapData" height="500px" />
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
import Log from '../components/Log';
import ListInfo from '../components/ListInfo';
import UserMap from '../components/UserMap';

export default {
    name: 'Home',
    components: {
        Log,
        ListInfo,
        UserMap,
    },
    computed: {
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
        gitUpdateURL() {
            return this.$store.state.gitDownloadUrl;
        },
        userMapData() {
            return this.$store.state.userMapData;
        },
        isDemoMode() {
            return this.$demoMode;
        },
    },
};
</script>
<style scoped>
.updateMessage {
    color: rgb(255, 102, 0);
    font-weight: bold;
}
</style>

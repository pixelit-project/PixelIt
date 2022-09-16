<template>
    <v-container class="home">
        <v-row>
            <v-col cols="12" lg="4">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Firmware</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <div class="text-center">
                        <v-file-input v-model="firmwareFile" prepend-icon="mdi-file-powerpoint-outline" show-size filled accept=".bin" label="Select firmware upload" dense></v-file-input>

                        <ButtonCondition color="success" :condition="sockedIsConnected && inputFWCheck" text="Update Firmware" icon="mdi-file-upload" :onclick="uploadFirmware" />
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Filesystem</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <div class="text-center">
                        <v-file-input v-model="filesystemFile" prepend-icon="mdi-file-powerpoint-outline" show-size filled accept=".bin" label="Select filesystem upload" dense></v-file-input>
                        <ButtonCondition color="success" :condition="sockedIsConnected && inputFSCheck" text="Update Filesystem" icon="mdi-file-upload" :onclick="uploadFilesystem" />
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Download Stats</h2>
                        <span class="smallInfo">( last 4 versions )</span>
                    </v-card-title>
                    <hr />
                    <DownloadStats :items="gitReleases" />
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
import DownloadStats from '../components/DownloadStats';
import ButtonCondition from '../components/ButtonCondition';
export default {
    name: 'Update',
    components: { DownloadStats, ButtonCondition },
    data() {
        return {
            firmwareFile: null,
            filesystemFile: null,
        };
    },
    computed: {
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        systemItems() {
            return this.$store.state.sysInfoData;
        },
        sensorItems() {
            return this.$store.state.sensorData;
        },
        gitReleases() {
            return this.$store.state.gitReleases;
        },
        inputFWCheck() {
            return this.firmwareFile != null && this.firmwareFile != undefined && this.firmwareFile != '';
        },
        inputFSCheck() {
            return this.filesystemFile != null && this.filesystemFile != undefined && this.filesystemFile != '';
        },
    },
    methods: {
        async uploadFirmware() {
            if (!this.firmwareFile) {
                return;
            }
            const formData = new FormData();
            formData.append('firmware', this.firmwareFile);
            try {
                this.firmwareFile = null;
                this.$socket.close();
                await fetch(`http://${this.$pixelitHost}/update`, { method: 'POST', body: formData });
            } catch (e) {
                e;
            }
        },
        async uploadFilesystem() {
            if (!this.filesystemFile) {
                return;
            }
            const formData = new FormData();
            formData.append('filesystem', this.filesystemFile);
            try {
                this.filesystemFile = null;
                this.$socket.close();
                await fetch(`http://${this.$pixelitHost}/update`, { method: 'POST', body: formData });
            } catch (e) {
                e;
            }
        },
    },
};
</script>

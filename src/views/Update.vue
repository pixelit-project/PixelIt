<template>
    <v-container class="Update">
        <v-row>
            <v-col cols="12" lg="8">
                <v-row>
                    <v-col cols="12" lg="12">
                        <v-card class="pa-0" elevation="4">
                            <v-card-text class="text-md-center">
                                <h2>Firmware / Filesystem Update</h2>
                            </v-card-text>
                        </v-card>
                    </v-col>
                </v-row>
                <v-row>
                    <v-col cols="12" lg="6">
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
                    <v-col cols="12" lg="6">
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
                </v-row>  
                <v-row>
                    <v-col cols="12" lg="12">
                        <v-card class="pa-0" elevation="4">
                            <v-card-text class="text-md-center">
                                <h2>Configuration backup and restore</h2>
                            </v-card-text>
                        </v-card>
                    </v-col>
                </v-row>
                <v-row>
                    <v-col cols="12" lg="12">
                        <v-card class="pa-2" elevation="4">
                            <v-card-title>
                                <h2>Configuration</h2>
                            </v-card-title>
                            <hr />
                            <br />
                            <div class="text-center">
                                <v-file-input v-model="configFile" prepend-icon="mdi-file-table-outline" show-size filled accept=".json" label="Select configuration file" dense></v-file-input>                       
                                <ButtonCondition color="warning" :condition="sockedIsConnected && inputConfigCheck" text="Restore" icon="mdi-file-upload" :onclick="uploadConfig" />
                                <span>&nbsp;&nbsp;&nbsp;&nbsp;</span>
                                <ButtonCondition color="success" :condition="sockedIsConnected" text="Backup" icon="mdi-file-download" :onclick="downloadConfig" />                      
                            </div>
                        </v-card>
                    </v-col>        
                </v-row>  
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
            configFile: null,
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
        inputConfigCheck() {
            return this.configFile != null && this.configFile != undefined && this.configFile != '';
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
                if (this.$pixelitHost == undefined || this.$pixelitHost == null || this.$pixelitHost == '') {
                    alert('No Pixelit Host defined!');
                } else {
                    await fetch(`http://${this.$pixelitHost}/update`, { method: 'POST', body: formData });
                }
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
                if (this.$pixelitHost == undefined || this.$pixelitHost == null || this.$pixelitHost == '') {
                    alert('No Pixelit Host defined!');
                } else {
                    await fetch(`http://${this.$pixelitHost}/update`, { method: 'POST', body: formData });
                }
            } catch (e) {
                e;
            }
        },
        uploadConfig() {               
            let reader = new FileReader();
            reader.onload = e => { 
                const config = JSON.parse(e.target.result);
                // Clean Config
                delete config.version;
                delete config.isESP8266;     
                this.$socket.sendObj({ setConfig: config });
                setTimeout(() => {
                    this.$socket.close();
                }, 3000);   
            };
            reader.readAsText(this.configFile);
        },
        async downloadConfig(){          
            try {
                //const jsonData = JSON.stringify(await (await fetch(`http://${this.$pixelitHost}/api/config`)).json());
                const jsonData = JSON.stringify(this.$store.state.configData);                
                const blob = new Blob([jsonData], { type: 'application/json' });
                const url = URL.createObjectURL(blob);

                const link = document.createElement('a');
                link.href = url;
                link.download = `pixlitConfig_${this.$store.state.version}.json`;

                link.click();

                URL.revokeObjectURL(url);
            } catch (error) {
                console.log(`downloadConfig: error (${error})`);
            }
        }  
    },
};

</script>

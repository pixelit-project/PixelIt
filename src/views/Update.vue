<template>
    <v-container class="Update">
        <v-row>
            <v-col cols="12" lg="8">
                <v-row>
                    <v-col cols="12" lg="12">
                        <v-card class="pa-0" elevation="4">
                            <v-card-text class="text-md-center">
                                <h2>Update Firmware & Filesystem</h2>
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
                    </v-card-title>
                    <hr />
                    <DownloadStats :items="gitReleases" />
                </v-card>
            </v-col>
        </v-row>     
        <v-dialog v-model="popupIsActive" max-width="500">
        <v-card>
            <v-card-title class="headline orange--text">Warning, please note!</v-card-title>
                <v-card-text>
                    <div v-for="restoreWarning in restoreWarnings" :key="restoreWarning">
                    {{ restoreWarning }}                    
                    </div>
                    <br>
                    Should the configuration backup still be restored on this PixelIt?
                </v-card-text>
            <v-card-actions>
            <v-spacer></v-spacer>
            <v-btn color="orange" text @click="uploadConfigWithWarnings">Ignore and Restore</v-btn>
            <v-btn color="green" text @click="popupIsActive = false">Cancel</v-btn>         
            </v-card-actions>
        </v-card>
    </v-dialog>
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
            popupIsActive: false,
            ignoreRestoreWarnings: false,
            restoreWarnings: []
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
        uploadConfigWithWarnings() {
            this.popupIsActive=false;
            this.ignoreRestoreWarnings= true;            
            this.uploadConfig();
        },
        uploadConfig() {               
            const reader = new FileReader();
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

            const version = this.$store.state.version;
            const buildSection = JSON.parse(this.$store.state.telemetryData).buildSection;

            const [, fileVersion, ...buildSectionParts] = this.configFile.name.replace(/\(\d+\)/g, '()').replace(/[\s()]+/g, '').replace('.json', '').split('_');
            const fileBuildSection = buildSectionParts.join('_');

            this.restoreWarnings = [];

            if (this.ignoreRestoreWarnings == false){

                if (fileVersion == ''){
                    this.restoreWarnings.push("It could not be determined with which firmware version this backup was created.");
                } 
                else if (version != fileVersion){
                    this.restoreWarnings.push("Backup firmware version does not match the version of this PixelIt!");
                    this.restoreWarnings.push(`Backup: ${fileVersion} <--> PixelIt: ${version}`);                                  
                }

                if (this.restoreWarnings.length > 0){
                    this.restoreWarnings.push(`---`);
                }
            
                if (fileBuildSection == ''){
                    this.restoreWarnings.push("It could not be determined with which hardware version this backup was created.");
                } 
                else if (buildSection != fileBuildSection){
                    this.restoreWarnings.push("Backup hardware version does not match the version of this PixelIt!");
                    this.restoreWarnings.push(`Backup: ${fileBuildSection} <--> PixelIt: ${buildSection}`);
                }
                
                if (this.restoreWarnings.length > 0){
                    this.popupIsActive = true;
                    return;
                }
            }

            this.ignoreRestoreWarnings = false;
            reader.readAsText(this.configFile);            
        },
        async downloadConfig(){          
            try {
                const jsonData = JSON.stringify(this.$store.state.configData);                
                const blob = new Blob([jsonData], { type: 'application/json' });
                const url = URL.createObjectURL(blob);

                const link = document.createElement('a');
                link.href = url;

                const version = this.$store.state.version;
                const buildSection = JSON.parse(this.$store.state.telemetryData).buildSection;

                link.download = `pixlitConfig_${version}_${buildSection}.json`;
                link.click();

                URL.revokeObjectURL(url);
            } catch (error) {
                console.log(`downloadConfig: error (${error})`);
            }
        }  
    },
};

</script>

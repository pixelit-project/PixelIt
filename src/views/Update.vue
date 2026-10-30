<template>
    <v-container class="Update app-page">
        <v-row>
            <v-col cols="12" lg="9">
                <v-card class="update-card mb-6" elevation="2">
                    <v-card-text>
                        <div class="version-grid">
                            <div><span class="update-label">Installed version</span><strong>{{ installedVersion || 'Unknown' }}</strong></div>
                            <div><span class="update-label">Latest stable release</span><strong>{{ latestRelease ? latestRelease.tag_name : 'Unavailable' }}</strong></div>
                            <div><span class="update-label">Detected hardware</span><strong>{{ hardware || 'Unknown' }}</strong></div>
                        </div>
                        <v-alert v-if="releaseError" class="mt-4 mb-0" type="warning" text dense>{{ releaseError }}</v-alert>
                    </v-card-text>
                    <v-divider />
                    <v-card-text class="download-row">
                        <div>
                            <div class="font-weight-medium text--primary">Get the right firmware</div>
                            <div class="text--secondary">Download the file for your hardware, then upload it below.</div>
                            <div v-if="latestRelease && !matchingAsset" class="mt-1">No matching firmware found. Check the release notes.</div>
                        </div>
                        <div class="download-actions">
                        <v-btn text small href="https://github.com/pixelit-project/PixelIt/releases" target="_blank" rel="noopener noreferrer">Release notes <v-icon right small>mdi-open-in-new</v-icon></v-btn>
                        <v-btn v-if="matchingAsset" color="primary" elevation="0" :href="matchingAsset.browser_download_url" target="_blank" rel="noopener noreferrer">
                            <v-icon left>mdi-download</v-icon> Download firmware
                        </v-btn>
                        </div>
                    </v-card-text>
                </v-card>
                <div v-if="updateMessage" class="mb-6">
                    <v-alert :type="updateStatus === 'error' ? 'error' : updateStatus === 'connected' ? 'success' : 'info'" role="status" aria-live="polite">{{ updateMessage }}</v-alert>
                    <v-progress-linear v-if="updateBusy" :value="uploadProgress" :indeterminate="updateStatus !== 'uploading'" height="24" color="primary">
                        <span v-if="updateStatus === 'uploading'">{{ uploadProgress }}%</span>
                    </v-progress-linear>
                </div>
                <v-row>
                    <v-col cols="12" md="6" class="d-flex">
                        <v-card class="update-card upload-card" elevation="2">
                            <v-card-title><v-icon left color="primary">mdi-chip</v-icon><h2>Firmware</h2></v-card-title>
                            <v-card-text>
                                <p>Update the software running on your PixelIt.</p>
                                <v-file-input v-model="firmwareFile" :disabled="updateBusy" prepend-icon="" prepend-inner-icon="mdi-file-upload-outline" show-size filled accept=".bin" label="Select firmware (.bin)" dense />
                            </v-card-text>
                            <v-card-actions>
                                <ButtonCondition color="success" :condition="canUpdate && inputFWCheck" text="Update Firmware" icon="mdi-file-upload" :onclick="uploadFirmware" />
                            </v-card-actions>
                        </v-card>
                    </v-col>
                    <v-col cols="12" md="6" class="d-flex">
                        <v-card class="update-card upload-card" elevation="2">
                            <v-card-title><v-icon left color="primary">mdi-folder-outline</v-icon><h2>Filesystem</h2></v-card-title>
                            <v-card-text>
                                <p>Upload a filesystem image when required by a release.</p>
                                <v-file-input v-model="filesystemFile" :disabled="updateBusy" prepend-icon="" prepend-inner-icon="mdi-file-upload-outline" show-size filled accept=".bin" label="Select filesystem (.bin)" dense />
                            </v-card-text>
                            <v-card-actions>
                                <ButtonCondition color="success" :condition="canUpdate && inputFSCheck" text="Update Filesystem" icon="mdi-file-upload" :onclick="uploadFilesystem" />
                            </v-card-actions>
                        </v-card>
                    </v-col>
                </v-row>
                <p class="text--secondary update-power-note"><v-icon small class="mr-1">mdi-power-plug-outline</v-icon> Keep the PixelIt powered on during the update.</p>
                <v-card class="update-card" elevation="2">
                    <v-card-title><v-icon left color="primary">mdi-backup-restore</v-icon><h2>Configuration backup &amp; restore</h2></v-card-title>
                    <v-card-text>
                        <p>Save your current settings before updating, or restore a previous backup.</p>
                        <p v-if="backupMessage" role="status">{{ backupMessage }}</p>
                        <div class="configuration-grid">
                            <div class="backup-action">
                                <ButtonCondition color="success" :condition="canUpdate" text="Download backup" icon="mdi-file-download" :onclick="downloadConfig" />
                                <span class="text--secondary">Save current settings as a JSON file.</span>
                            </div>
                            <div>
                                <v-file-input :disabled="updateBusy" v-model="configFile" prepend-icon="" prepend-inner-icon="mdi-file-upload-outline" show-size filled accept=".json" label="Select backup (.json)" dense />
                                <ButtonCondition color="warning" :condition="canUpdate && inputConfigCheck" text="Restore backup" icon="mdi-file-upload" :onclick="uploadConfig" />
                            </div>
                        </div>
                    </v-card-text>
                </v-card>
            </v-col>
            <v-col cols="12" lg="3">
                <v-card class="update-card" elevation="2">
                    <v-card-title><h2>Release history</h2></v-card-title>
                    <v-card-subtitle>Downloads &amp; changes by version</v-card-subtitle>
                    <DownloadStats :items="gitReleases" />
                </v-card>
            </v-col>
        </v-row>
        <v-dialog v-model="updateDialog" max-width="600">
            <v-card>
                <v-card-title>Prepare {{ pendingKind }} update</v-card-title>
                <v-card-text>
                    <p>Selected file: {{ pendingFile ? pendingFile.name : '' }}</p>
                    <v-alert v-if="fileWarning" type="warning" text>{{ fileWarning }}</v-alert>
                    <v-checkbox v-if="fileWarning && !fileBlocked" v-model="acceptFileWarning" label="I have verified that this file is compatible." />
                    <p>Save a configuration backup before updating.</p>
                    <v-btn color="primary" outlined :disabled="!canUpdate" @click="downloadConfig">Download backup</v-btn>
                    <p v-if="backupMessage" class="mt-3" role="status">{{ backupMessage }}</p>
                </v-card-text>
                <v-card-actions class="flex-wrap">
                    <v-spacer />
                    <v-btn text @click="updateDialog = false">Cancel</v-btn>
                    <v-btn color="success" :disabled="!canUpdate || fileBlocked || (!!fileWarning && !acceptFileWarning)" @click="startUpdate">Start update</v-btn>
                </v-card-actions>
            </v-card>
        </v-dialog>
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
import { notify } from '../utils/feedback';
import ButtonCondition from '../components/ButtonCondition';
import { firmwareWarning, releaseAsset } from '../utils/update';
export default {
    name: 'Update',
    components: { DownloadStats, ButtonCondition },
    data() {
        return {
            latestRelease: null,
            releaseError: '',
            updateDialog: false,
            pendingKind: '',
            pendingFile: null,
            acceptFileWarning: false,
            backupMessage: '',
            updateStatus: '',
            updateMessage: '',
            uploadProgress: 0,
            restartTimer: null,
            restartDisconnected: false,
            firmwareFile: null,
            filesystemFile: null,
            configFile: null,
            popupIsActive: false,
            ignoreRestoreWarnings: false,
            restoreWarnings: []
        };
    },
    created() {
        this.loadLatestRelease();
    },
    beforeDestroy() {
        clearTimeout(this.restartTimer);
        if (this.uploadRequest) this.uploadRequest.abort();
    },
    watch: {
        sockedIsConnected(connected) {
            if (this.updateStatus !== 'restarting') return;
            if (!connected) this.restartDisconnected = true;
            if (connected && this.restartDisconnected) {
                clearTimeout(this.restartTimer);
                this.updateStatus = 'connected';
                this.updateMessage = 'PixelIt is connected again. Check the installed version above to verify the update.';
            }
        },
    },
    computed: {
        installedVersion() { return this.$store.state.version; },
        hardware() {
            try {
                const telemetry = this.$store.state.telemetryData;
                return (typeof telemetry === 'string' ? JSON.parse(telemetry) : telemetry).buildSection || '';
            } catch (error) { return ''; }
        },
        matchingAsset() { return releaseAsset(this.latestRelease, this.hardware); },
        updateBusy() { return ['uploading', 'installing', 'restarting'].includes(this.updateStatus); },
        canUpdate() { return this.sockedIsConnected && !this.updateBusy && !this.$demoMode; },
        fileWarning() { return firmwareWarning(this.pendingFile, this.hardware, this.pendingKind); },
        fileBlocked() {
            return !this.pendingFile || !/\.bin$/i.test(this.pendingFile.name) || !this.pendingFile.size || /\.full-upgrade\.bin$/i.test(this.pendingFile.name) || (this.pendingKind === 'filesystem' && /^firmware_v/i.test(this.pendingFile.name));
        },
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
        async loadLatestRelease() {
            const controller = new AbortController();
            const timer = setTimeout(() => controller.abort(), 15000);
            try {
                const response = await fetch('https://api.github.com/repos/pixelit-project/PixelIt/releases/latest', { signal: controller.signal });
                if (!response.ok) throw new Error(`HTTP ${response.status}`);
                const release = await response.json();
                if (!release.tag_name || !Array.isArray(release.assets)) throw new Error('Invalid release data');
                this.latestRelease = release;
            } catch (error) {
                this.releaseError = 'Latest release could not be loaded. Use the releases link to download firmware manually.';
            } finally { clearTimeout(timer); }
        },
        uploadFirmware() { this.prepareUpdate('firmware', this.firmwareFile); },
        uploadFilesystem() { this.prepareUpdate('filesystem', this.filesystemFile); },
        prepareUpdate(kind, file) {
            if (!this.canUpdate || !file) return;
            this.pendingKind = kind;
            this.pendingFile = file;
            this.acceptFileWarning = false;
            this.backupMessage = '';
            this.updateDialog = true;
        },
        startUpdate() {
            if (!this.canUpdate || this.fileBlocked || (this.fileWarning && !this.acceptFileWarning)) return;
            if (!this.$pixelitHost) {
                this.updateStatus = 'error';
                this.updateMessage = 'No PixelIt host defined.';
                this.updateDialog = false;
                return;
            }
            const formData = new FormData();
            formData.append(this.pendingKind, this.pendingFile);
            this.updateDialog = false;
            this.uploadProgress = 0;
            this.updateStatus = 'uploading';
            this.updateMessage = 'Uploading update. Keep the PixelIt powered on.';
            const request = new XMLHttpRequest();
            this.uploadRequest = request;
            request.open('POST', `http://${this.$pixelitHost}/update`);
            request.timeout = 180000;
            request.upload.onprogress = event => {
                if (event.lengthComputable) this.uploadProgress = Math.round(event.loaded / event.total * 100);
            };
            request.upload.onload = () => {
                this.updateStatus = 'installing';
                this.updateMessage = 'Upload transferred. Waiting for the PixelIt to finish installing.';
            };
            const fail = message => {
                this.uploadRequest = null;
                this.updateStatus = 'error';
                this.updateMessage = message;
            };
            request.onerror = () => fail('Connection lost during the update. The result is unknown. Check the PixelIt and its installed version before retrying.');
            request.ontimeout = () => fail('Update timed out. Check the PixelIt and its installed version before retrying.');
            request.onload = () => {
                this.uploadRequest = null;
                if (request.status < 200 || request.status >= 300 || /fail|error/i.test(request.responseText)) {
                    fail(`Update rejected by PixelIt (HTTP ${request.status}). Check that the image is compatible.`);
                    return;
                }
                this.updateStatus = 'restarting';
                this.updateMessage = 'Update accepted. Waiting for restart and reconnection…';
                this.restartDisconnected = !this.sockedIsConnected;
                this.restartTimer = setTimeout(() => {
                    this.updateStatus = 'error';
                    this.updateMessage = 'Reconnection was not confirmed. Check power and network, then reload this page to verify the installed version.';
                }, 90000);
                if (this.pendingKind === 'firmware') this.firmwareFile = null;
                else this.filesystemFile = null;
                // The websocket plugin automatically reconnects after closing.
                if (this.$socket) this.$socket.close();
            };
            try { request.send(formData); }
            catch (error) { fail('Could not start the upload. Check the PixelIt connection and try again.'); }
        },
        uploadConfigWithWarnings() {
            this.popupIsActive=false;
            this.ignoreRestoreWarnings= true;            
            this.uploadConfig();
        },
        uploadConfig() {               
            const reader = new FileReader();
            reader.onerror = () => notify('Could not read the configuration file.', 'error');
            reader.onload = e => {
                try {
                const config = JSON.parse(e.target.result);
                if (!config || typeof config !== 'object' || Array.isArray(config)) throw new Error('Invalid configuration');
                if (!this.canUpdate) throw new Error('PixelIt is offline. Reconnect before restoring.');
                // Clean Config
                delete config.version;
                delete config.isESP8266;     
                this.$socket.sendObj({ setConfig: config });
                setTimeout(() => {
                    this.$socket.close();
                }, 3000);
                notify('Configuration restore sent. PixelIt will restart; reconnect to verify your settings.', 'success');
                } catch (error) { notify('Restore failed. Check the JSON file and device connection.', 'error'); }
            };

            const version = this.$store.state.version;
            const buildSection = this.hardware || 'unknown';

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
                if (!this.canUpdate || !Object.keys(this.$store.state.configData || {}).length) throw new Error('Configuration unavailable');
                const jsonData = JSON.stringify(this.$store.state.configData);                
                const blob = new Blob([jsonData], { type: 'application/json' });
                const url = URL.createObjectURL(blob);

                const link = document.createElement('a');
                link.href = url;

                const version = this.$store.state.version;
                const buildSection = this.hardware || 'unknown';

                link.download = `pixlitConfig_${version}_${buildSection}.json`;
                link.click();

                setTimeout(() => URL.revokeObjectURL(url), 1000);
                this.backupMessage = 'Backup download requested. Make sure the file is saved before starting the update.';
            } catch (error) {
                this.backupMessage = 'Backup could not be created. Please try again.';
            }
        }  
    },
};

</script>

<style scoped>
.Update { max-width: 1920px; padding: 20px 16px; }
.update-card { border-radius: 10px; overflow: hidden; }
.update-card h2 { font-size: 18px; font-weight: 500; line-height: 1.4; }
.update-card > .v-card__title { padding: 20px 24px 12px; word-break: normal; }
.update-card > .v-card__subtitle { padding-left: 24px; padding-right: 24px; }
.update-card > .v-card__text { padding: 20px 24px; }
.version-grid { display: grid; grid-template-columns: 1fr 1fr 1.4fr; gap: 24px; }
.version-grid strong { display: block; font-size: 18px; font-weight: 500; overflow-wrap: anywhere; color: var(--v-primary-base); }
.update-label { display: block; margin-bottom: 6px; font-size: 12px; }
.download-actions { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; }
.download-row { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 16px; }
.upload-card { display: flex; flex-direction: column; width: 100%; }
.upload-card > .v-card__text { flex: 1; padding-top: 0; padding-bottom: 0; }
.upload-card > .v-card__actions { padding: 0 24px 24px; }
.update-power-note { font-size: 12px; margin-top: 12px; margin-bottom: 24px; }
.configuration-grid { display: grid; grid-template-columns: 1fr 1.4fr; gap: 24px; margin-top: 24px; }
.backup-action { display: flex; flex-direction: column; align-items: flex-start; gap: 12px; }
@media (max-width: 599px) {
    .Update { padding: 16px 12px; }
    .version-grid, .configuration-grid { grid-template-columns: 1fr; gap: 16px; }
    .update-card > .v-card__text { padding: 16px; }
    .update-card > .v-card__title { padding: 16px; }
    .upload-card > .v-card__actions { padding: 0 16px 16px; }
}
</style>

<template>
    <v-container class="statistics app-page">
        <v-alert v-if="statsError && sendStatistics !== false" type="error" text role="alert">Statistics could not be loaded. Check the connection and reload this page to try again.</v-alert>
        <div v-else-if="sendStatistics === false">
            <v-row>
                <v-col cols="12" lg="12">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-text class="text-md-center">
                            <h2>Activate the telemetry data to see the statistics.</h2>
                            <br>
                            <h4>To activate the telemetry data, go to <a href="/#/options"><b>Options</b> and activate <b>"Send Telemetry data"</b></a></h4>
                            <small>After activation, it can take up to one minute for the data to become visible.</small>
                        </v-card-text>
                    </v-card>
                </v-col>
            </v-row>
        </div>
        <v-skeleton-loader v-else-if="!statsReady" type="article, image, image" aria-label="Loading statistics" />
        <div v-else>
            <v-alert v-if="!countryStats.length" type="info" text>No country statistics are available yet.</v-alert>
            <v-row>
                <v-col cols="12" lg="12">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-text class="text-md-center">
                            These statistics are created with the telemetry data of the PixelIts that have reported in the last 30 days.<br>
                                <b>Thanks to everyone who shares this data with us!</b>
                        </v-card-text>
                    </v-card>
                </v-col>
            </v-row>
            <v-row>
                <v-col cols="12" lg="4">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-title>
                            <h2>Boards</h2>
                        </v-card-title>
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="buildStats"></apexchart>
                    </v-card>                   
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-title>
                            <h2>Versions</h2>
                        </v-card-title>
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="versionStats"></apexchart>
                    </v-card>
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-title>
                            <h2>Sensors</h2>
                        </v-card-title>
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="sensorStats"></apexchart>
                    </v-card>
                </v-col>
            </v-row>
            <v-row>
                <v-col cols="12" lg="8">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-title>
                            <h2>Countries</h2>
                        </v-card-title>
                        <apexchart height="600px" type="donut" :options="countryChartOptions" :series="countryStats"></apexchart>
                    </v-card>               
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-title>
                            <h2>Matrix Types</h2>
                        </v-card-title>
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="matrixStats"></apexchart>
                    </v-card>
                </v-col>
            </v-row>
            <v-row>
                <v-col cols="12" lg="12">
                    <v-card class="ui-panel" elevation="2">
                        <v-card-title>
                            <h2>Usermap</h2>
                        </v-card-title>
                        <p></p>
                        <UserMap :mapZoom= 3  :coords="userMapData" height="600px" />
                    </v-card>
                </v-col>
            </v-row>
        </div>
    </v-container>
</template>

<script>
import UserMap from '../components/UserMap';

// Explicit colors survive ApexCharts theme updates during asynchronous data loads.
const statisticsColors = ['#A300D6', '#7D02EB', '#5653FE', '#2983FF', '#00B1F2'];

export default {
    name: 'Statistics',
     data: () => ({        
    }),
    components: {
        UserMap,
    },
    computed: {
        statsReady() {
            return ['buildStats', 'versionStats', 'sensorStats', 'countryStats', 'matrixStats'].every(key => Array.isArray(this.$store.state.statistics[key]));
        },
        userMapData() {
            return this.$store.state.userMapData.coords;
        },
        buildStats() {
            if (this.$store.state.statistics.buildStats){
                return [ { name: 'Count', data: this.$store.state.statistics.buildStats
                    .map(({build: x, count})=>({count, x}))
                    .map(({count: y, x})=>({y, x}))}];
            }
            return [];           
        },  
        versionStats() {
            if (this.$store.state.statistics.versionStats){
                return [ { name: 'Count', data: this.$store.state.statistics.versionStats
                    .map(({version: x, count})=>({count, x}))
                    .map(({count: y, x})=>({y, x}))}];
            }
            return [];           
        }, 
        sensorStats() {
            if (this.$store.state.statistics.sensorStats){
                return [ { name: 'Count', data: this.$store.state.statistics.sensorStats
                    .map(({sensor: x, count})=>({count, x}))
                    .map(({count: y, x})=>({y, x}))}];
            }
            return [];           
        }, 
        matrixStats() {
            if (this.$store.state.statistics.matrixStats){
                return [ { name: 'Count', data: this.$store.state.statistics.matrixStats
                    .map(({matrix: x, count})=>({count, x}))
                    .map(({count: y, x})=>({y, x}))}];
            }
            return [];           
        }, 
        countryStats() {
            if (this.$store.state.statistics.countryStats){
                return  this.$store.state.statistics.countryStats.map(x=> x.count) ;
            }
            return [];           
        },
        countryChartOptions(){
            return { 
                colors: statisticsColors.slice(),
                labels: this.$store.state.statistics.countryStats ? this.$store.state.statistics.countryStats.map(x=> x.country) : [],
                theme: {
                    mode: this.$vuetify.theme.dark ? 'dark' : 'light',
                    palette: 'palette10',
                },
                chart: {
                    background: '#00000000',
                    dropShadow: {
                        enabled: false,
                    }
                }, 
                plotOptions: {
                    pie: {                        
                        expandOnClick: true,
                        donut: {
                            size: '0%',
                            labels: {                               
                                show: false,
                            }
                        }
                    }
                }
            };
        },
        chartOptions(){
            return {
                colors: statisticsColors.slice(),
                plotOptions: {
                    bar: {
                        horizontal: false,
                        distributed: true,         
                    },
                },
                dataLabels: {
                    enabled: true,
                },
                legend: {
                    show: false,
                },
                theme: {
                    mode: this.$vuetify.theme.dark ? 'dark' : 'light',
                    palette: 'palette10',
                },
                chart: {
                    background: '#00000000',
                    dropShadow: {
                        enabled: false,
                    }
                } 
            };
        },  
        statsError() {            
            return this.$store.state.statistics.error;             
        },  
        sendStatistics(){
            return this.$store.state.configData.sendTelemetry;
        }
    },
    methods: {       
    },
    watch: {        
    },
};
</script>
<style scoped>
ul {
    margin-top: 10px;
}
li {
    margin-left: 20px;
}
</style>

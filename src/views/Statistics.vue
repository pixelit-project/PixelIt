<template>
    <v-container class="statistics">
        <div v-if="statsError || sendStatistics == false">
            <v-row>
                <v-col cols="12" lg="12">
                    <v-card class="pa-2" elevation="4">
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
        <div v-else>
            <v-row>
                <v-col cols="12" lg="12">
                    <v-card class="pa-2" elevation="4">
                        <v-card-text class="text-md-center">
                            These statistics are created with the telemetry data of the PixelIts that have reported in the last 30 days.<br>
                                <b>Thanks to everyone who shares this data with us!</b>
                        </v-card-text>
                    </v-card>
                </v-col>
            </v-row>
            <v-row>
                <v-col cols="12" lg="4">
                    <v-card class="pa-1" elevation="4">
                        <v-card-title>
                            <h2>Boards</h2>
                        </v-card-title>
                        <hr />              
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="buildStats"></apexchart>
                    </v-card>                   
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Versions</h2>
                        </v-card-title>
                        <hr />                      
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="versionStats"></apexchart>                  
                    </v-card>
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Sensors</h2>
                        </v-card-title>
                        <hr />                      
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="sensorStats"></apexchart>                  
                    </v-card>
                </v-col>
            </v-row>
            <v-row>
                <v-col cols="12" lg="8">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Countries</h2>
                        </v-card-title>
                        <hr />                    
                        <apexchart height="600px" type="donut" :options="countryChartOptions" :series="countryStats"></apexchart>     
                    </v-card>               
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Matrix Types</h2>
                        </v-card-title>
                        <hr />                      
                        <apexchart height="600px" width="100%" type="bar" :options="chartOptions" :series="matrixStats"></apexchart>                  
                    </v-card>
                </v-col>
            </v-row>
            <v-row>
                <v-col cols="12" lg="12">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Usermap</h2>
                        </v-card-title>
                        <hr />                        
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

export default {
    name: 'Statistics',
     data: () => ({        
    }),
    components: {
        UserMap,
    },
    computed: {
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
<style>
ul {
    margin-top: 10px;
}
li {
    margin-left: 20px;
}
</style>

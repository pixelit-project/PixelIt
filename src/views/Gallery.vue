<template>
    <v-container class="gallery">
        <v-row>
            <v-col cols="12">
                <v-text-field v-model="message" v-if="!isLoading" prepend-inner-icon="mdi-magnify" single-line outlined filled hide-details auto-grow label="Search for name or ID" type="text"></v-text-field>
            </v-col>
            <v-col v-if="isLoading" cols="12" class="text-center">
                <fold :loading="isLoading"></fold>
                Loading...
            </v-col>
            <v-col v-for="bmp in getBMPs" :key="bmp.id" cols="6" lg="2">
                <v-card class="pa-2" elevation="4">
                    <div class="text-center">
                        {{ bmp.name }}
                    </div>
                    <hr />
                    <p></p>
                    <BmpCanvas :bmp="bmp" />
                    <p></p>
                    <!-- <hr /> -->
                    <div class="text-center">
                        <BmpDialog :bmp="bmp" />
                        <v-btn icon @click="sendBitmap(bmp.rgB565Array, bmp.sizeX)" :disabled="!sockedIsConnected" class="float-right" title="Show on PixelIt">
                            <v-icon>mdi-arrow-right-circle-outline</v-icon>
                        </v-btn>
                    </div>
                    <div class="text-center">
                        <v-text-field prepend-inner-icon="mdi-identifier" rounded dense hide-details readonly :value="bmp.id" append-outer-icon="mdi-content-copy" @click:append-outer="copyText(bmp.id, $event)"></v-text-field>
                    </div>
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
import BmpCanvas from '../components/BmpCanvas';
import BmpDialog from '../components/BmpDialog';
export default {
    name: 'Home',
    created: function () {
        getBMPsFromAPI(this.$store.state);
    },
    data: function () {
        return {
            message: '',
        };
    },
    components: {
        BmpCanvas,
        BmpDialog,
    },
    methods: {
        copyText(value, event) {
            navigator.clipboard.writeText(value);
            console.log(event);
            setTimeout(() => {
                event.target.blur();
            }, 200);
        },
        sendBitmap(rgB565Array, sizeX) {
            if (rgB565Array.endsWith(',')) {
                rgB565Array = rgB565Array.slice(0, -1);
            }
            if (sizeX == 8) {
                this.$socket.sendObj({
                    setScreen: {
                        bitmapAnimation: {
                            data: JSON.parse(`[${rgB565Array}]`),
                            animationDelay: 200,
                        },
                    },
                });
            } else {
                this.$socket.sendObj({                   
                    setScreen: {
                        bitmap: {
                            data: JSON.parse(rgB565Array),
                            position: {
                                x: 0,
                                y: 0,
                            },
                            size: {
                                width: sizeX,
                                height: 8,
                            },
                        },
                    },
                });
            }
        },
    },
    computed: {
        getBMPs() {
            return this.$store.state.bmpsFromAPI.filter((x) => x.name.toLowerCase().includes(this.message.toLowerCase()) || x.id == this.message);
        },
        isLoading() {
            return this.$store.state.bmpsFromAPI.length == 0;
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
    },
};

async function getBMPsFromAPI(state) {
    try {
        state.bmpsFromAPI = await (await fetch('https://pixelit.bastelbunker.de/api/GetBMPAll')).json();
    } catch (error) {
        console.log(`getBMPsFromAPI: error (${error})`);
        state.bmpsFromAPI = [];
    }
}
</script>

<style scoped>
canvas {
    padding-left: 0;
    padding-right: 0;
    margin-left: auto;
    margin-right: auto;
    display: block;
    border: 1px solid;
    border-color: grey;
}
</style>

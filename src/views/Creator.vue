<template>
    <v-container class="gallery">
        <v-row>
            <v-col cols="12" lg="12">
                <v-bottom-navigation :value="pixelMode" color="primary">
                    <v-btn @click="changePixelModeTo8x8()">
                        <span>8x8 Pixel</span>
                        <v-icon>mdi-grid</v-icon>
                    </v-btn>

                    <v-btn @click="changePixelModeTo8x32()">
                        <span>8x32 Pixel</span>
                        <v-icon>mdi-grid</v-icon>
                    </v-btn>
                </v-bottom-navigation>
            </v-col>
        </v-row>
        <v-row v-if="pixelMode == 0">
            <v-col cols="12" lg="4" offset-lg="2">
                <v-card class="pa-3" elevation="4">
                    <Art :colors="colors" pixelCount="64" :func="onclick" :backgrounds="active8x8Background" />
                    <p></p>
                    <v-textarea filled outlined v-model="array8x8String" rows="5" hide-details></v-textarea>
                    <v-switch v-model="livedraw" label="Live draw" hide-details dense></v-switch>
                    <div class="text-center">
                        <ButtonSave color="green" text="Save" icon="mdi-content-save" :data="array8x8String" :pixelMode="this.pixelMode" :condition="true"></ButtonSave>
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="pa-3" elevation="4">
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="280"></v-color-picker>
                </v-card>
            </v-col>
        </v-row>
        <v-row v-if="pixelMode == 1">
            <v-col cols="12" lg="8" offset-lg="0">
                <v-card class="pa-3" elevation="4">
                    <Art :colors="colors" pixelCount="256" :func="onclick" :backgrounds="active8x32Background" />
                    <p></p>
                    <v-textarea filled outlined v-model="array8x32String" rows="9" hide-details></v-textarea>
                    <v-switch v-model="livedraw" label="Live draw" :disabled="!sockedIsConnected" hide-details dense></v-switch>
                    <div class="text-center" v-if="isAnimated" >
                        <v-card-text><h3 class="red--text">No animated 8x32 bitmaps are supported!</h3></v-card-text>
                    </div>
                    <div class="text-center">
                        <ButtonSave color="green" text ="Save" icon="mdi-content-save" :data="array8x32String" :pixelMode="pixelMode" :condition="!isAnimated"></ButtonSave>
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="pa-3" elevation="4">
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="270"></v-color-picker>
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>
<script>
import Art from '../components/Art';
import ButtonSave from '../components/ButtonBMPSave';
export default {
    data() {
        return {
            colors: '#F44336',
            pixelMode: 0,
            active8x8Background: Array.from({ length: 64 }, () => '#000000'),
            active8x32Background: Array.from({ length: 256 }, () => '#000000'),
            array8x8String: `[${Array.from({ length: 64 }, () => 0).join(',')}]`,
            array8x32String: `[${Array.from({ length: 256 }, () => 0).join(',')}]`,
            livedraw: false,
        };
    },
    components: {
        Art,
        ButtonSave,
    },
    computed: {
        cleaned8x32String(){
            return this.array8x32String.replaceAll('\n', '').replaceAll(' ', '');
        },
        isAnimated(){
            return this.cleaned8x32String.includes('],[');
        },
    },
    watch: {
        array8x8String(value) {
            this.active8x8Background = parseBitmapString(value, 64);
        },
        array8x32String(value) {
            this.active8x32Background = parseBitmapString(value, 256);
        },
    },
    methods: {
        onclick(id, color) {
            if (this.pixelMode == 0) {
                this.$set(this.active8x8Background, id - 1, color);
                this.array8x8String = bitmapArrayToString(this.active8x8Background);
            } else {
                this.$set(this.active8x32Background, id - 1, color);
                this.array8x32String = bitmapArrayToString(this.active8x32Background);
            }

            if (this.livedraw) {
                let screen;
                if (this.pixelMode == 0) {
                    screen = {
                        bitmapAnimation: {
                            data: JSON.parse(`[${this.array8x8String}]`),
                            animationDelay: 200,
                        },
                    };
                } else {
                    screen = {
                        bitmap: {
                            data: JSON.parse(this.array8x32String),
                            position: {
                                x: 0,
                                y: 0,
                            },
                            size: {
                                width: 32,
                                height: 8,
                            },
                        },
                    };
                }

                this.$socket.sendObj({
                    setScreen: screen,
                });
            }
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        changePixelModeTo8x8() {
            this.pixelMode = 0;
        },
        changePixelModeTo8x32() {
            this.pixelMode = 1;
        },
    },
};

function parseBitmapString(value, size) {
    const matches = value.match(/\d+/g);
    const parsedValues = matches ? matches.slice(0, size).map((number) => rgb565ToHex(Number(number))) : [];

    return Array.from({ length: size }, (_, index) => parsedValues[index] || '#000000');
}

function bitmapArrayToString(backgrounds) {
    const values = backgrounds.map((hex) => rgb888ToRgb565(hexToRgb(hex.replace('#', ''))));

    return `[${values.join(',')}]`;
}

function rgb888ToRgb565(rgbArray) {
    return ((rgbArray.r & 0xf8) << 8) + ((rgbArray.g & 0xfc) << 3) + (rgbArray.b >> 3);
}

function rgb565ToHex(rgb565) {
    const red = ((rgb565 >> 11) & 0x1f) << 3;
    const green = ((rgb565 >> 5) & 0x3f) << 2;
    const blue = (rgb565 & 0x1f) << 3;

    return `#${[red, green, blue]
        .map((value) => value.toString(16).padStart(2, '0'))
        .join('')}`;
}

function hexToRgb(hex) {
    const shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
    hex = hex.replace(shorthandRegex, function (m, r, g, b) {
        return r + r + g + g + b + b;
    });

    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result
        ? {
              r: parseInt(result[1], 16),
              g: parseInt(result[2], 16),
              b: parseInt(result[3], 16),
          }
        : null;
}
</script>
<style scoped>
#art,
.v-color-picker {
    padding-left: 0;
    padding-right: 0;
    margin-left: auto;
    margin-right: auto;
    display: block;
}
</style>

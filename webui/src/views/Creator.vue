<template>
    <v-container class="gallery">
        <v-row>
            <v-col cols="12" lg="3" offset-lg="3">
                <v-card class="pa-3" elevation="4">
                    <Art :colors="colors" :func="onclick" />
                    <p></p>
                    <v-textarea filled outlined v-model="arrayString" rows="5" hide-details></v-textarea>
                    <v-switch v-model="livedraw" label="Live draw" hide-details dense></v-switch>
                </v-card>
            </v-col>
            <v-col cols="12" lg="3">
                <v-card class="pa-3" elevation="4">
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="240"></v-color-picker>
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>
<script>
import Art from "../components/Art.vue";
export default {
    data() {
        return {
            colors: "#F44336",
            activeBackground: {},
            arrayString: "",
            livedraw: false,
        };
    },
    components: {
        Art,
    },
    methods: {
        onclick(id, color) {
            this.activeBackground[id] = color.replace("#", "");
            this.arrayString = "[";
            for (const hex of Object.values(this.activeBackground)) {
                this.arrayString += rgb888ToRgb565(hexToRgb(hex)) + ",";
            }
            this.arrayString = this.arrayString.slice(0, -1);
            this.arrayString += "]";

            if (this.livedraw) {
                this.$socket.sendObj({
                    setScreen: {
                        bitmapAnimation: {
                            data: JSON.parse(`[${this.arrayString}]`),
                            animationDelay: 200,
                        },
                    },
                });
            }
        },
    },
};

function rgb888ToRgb565(rgbArray) {
    return ((rgbArray.r & 0xf8) << 8) + ((rgbArray.g & 0xfc) << 3) + (rgbArray.b >> 3);
}

function hexToRgb(hex) {
    const shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
    hex = hex.replace(shorthandRegex, function(m, r, g, b) {
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

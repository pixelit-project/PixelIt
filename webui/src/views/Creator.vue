<template>
    <v-container class="gallery">
        <v-row>
            <v-col cols="12" lg="3" offset-lg="4">
                <v-card class="pa-3" elevation="4">
                    <Art :colors="colors" />
                    <p></p>
                    <v-textarea filled outlined rows="5"></v-textarea>
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="100"></v-color-picker>
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
            colors: "#00ff00",
        };
    },
    methods: {
        changeColor() {
            console.log("Change color");
            this.coloring = this.colors;
        },
    },
    components: {
        Art,
    },
};
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

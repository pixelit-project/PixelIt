<template>
    <div id="art" :style="{ width: getWidth(), height: getHeight() }">
        <Pixel :coloring="color" :id="n" :func="func" :pixelDimensions="getPixelDimensions()" v-for="n in getPixelCount()" :key="n" />
    </div>
</template>
<script>
import Pixel from "./Pixel.vue";
export default {
    computed: {
        color() {
            return this.colors;
        },
    },
    props: {
        colors: {
            type: String,
            required: true,
        },
        pixelCount: {
            type: String,
            required: true,
        },
        func: {
            type: Function,
            required: true,
        },
    },
    components: {
        Pixel,
    },
    methods: {
        getPixelCount() {
            return Number(this.pixelCount);
        },
        getWidth() {
            if (this.pixelCount == 64) {
                return "322px";
            } else {
                return "802px";
            }
        },
        getHeight() {
            if (this.pixelCount == 64) {
                return "322px";
            } else {
                return "202px";
            }
        },
        getPixelDimensions() {
            if (this.pixelCount == 64) {
                return "40";
            } else {
                return "25";
            }
        },
    },
};
</script>
<style scoped>
#art {
    /* width: 322px;*/
    /* height: 322px; */
    border: 1px solid rgb(90, 90, 90);
}
</style>

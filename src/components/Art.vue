<template>
    <div id="art" :style="{ width: getWidth(), height: getHeight() }">
        <Pixel :coloring="color" :id="p" :func="func" :pixelDimensions="getPixelDimensions()" v-for="p in getPixelCount()" :key="p" />
    </div>
</template>
<script>
import Pixel from './Pixel.vue';
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
                return '323px';
            } else {
                return '803px';
            }
        },
        getHeight() {
            if (this.pixelCount == 64) {
                return '323px';
            } else {
                return '202px';
            }
        },
        getPixelDimensions() {
            if (this.pixelCount == 64) {
                return '40';
            } else {
                return '25';
            }
        },
    },
};
</script>
<style scoped>
#art {
    border: 1px solid rgb(90, 90, 90);
}
</style>

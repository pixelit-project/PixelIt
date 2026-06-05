<template>
    <div class="pixel" :id="id" :style="{ background: background, height: getPixel(), width: getPixel() }" @click="changeColor"></div>
</template>
<script>
export default {
    props: {
        background: {
            type: String,
            required: true,
        },
        coloring: {
            type: String,
            required: true,
        },
        id: {
            type: Number,
            required: true,
        },
        func: {
            type: Function,
            required: true,
        },
        pixelDimensions: {
            type: String,
            required: true,
        },
    },
    methods: {
        changeColor() {
            const activeBackground = this.background === this.coloring ? '#000000' : this.coloring;
            this.func(this.id, activeBackground);
        },
        getPixel() {
            return this.pixelDimensions + 'px';
        },
    },
};
</script>
<style scoped>
div.pixel {
    border: 1px solid rgb(90, 90, 90);
    height: 40px;
    width: 40px;
    float: left;
}
</style>

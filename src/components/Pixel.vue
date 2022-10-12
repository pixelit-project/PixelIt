<template>
    <div class="pixel" :id="id" :style="{ background: activeBackground, height: getPixel(), width: getPixel() }" @click="changeColor"></div>
</template>
<script>
export default {
    data() {
        return {
            activeBackground: '#000',
        };
    },
    created() {
        this.func(this.id, this.activeBackground);
    },
    props: {
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
            if (this.activeBackground == this.coloring) {
                this.activeBackground = '#000';
            } else {
                this.activeBackground = this.coloring;
            }
            this.func(this.id, this.activeBackground);
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

<template>
    <div class="pixel" v-bind:style="{ background: activeBackground }" @click="changeColor"></div>
</template>
<script>
export default {
    data() {
        return {
            activeBackground: "#000",
        };
    },
    methods: {
        changeColor() {
            if (this.activeBackground == this.coloring) {
                this.activeBackground = "#000";
            } else {
                this.activeBackground = this.coloring;
            }
        },
    },
    props: {
        coloring: {
            type: String,
            required: true,
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

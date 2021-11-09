<template>
    <div id="art">
        <Pixel :coloring="color" :id="n" :func="func" v-for="n in 64" :key="n" />
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
        func: {
            type: Function,
            required: true,
        },
    },
    components: {
        Pixel,
    },
};
</script>
<style scoped>
#art {
    width: 322px;
    height: 322px;
    border: 1px solid rgb(90, 90, 90);
}
</style>

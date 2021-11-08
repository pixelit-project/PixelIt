<template>
    <div id="art">
        <div v-for="n in 64" :key="n">
            <Pixel :coloring="color" />
        </div>
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

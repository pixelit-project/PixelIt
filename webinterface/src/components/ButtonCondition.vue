<template>
    <v-btn v-if="condition" :color="color" elevation="4" @click="onclick">
        <v-icon left>{{ icon }}</v-icon>
        {{ text }}
    </v-btn>
    <v-btn v-else :color="color" elevation="4" disabled>
        <v-icon left>{{ icon }}</v-icon>
        {{ text }}
    </v-btn>
</template>

<script>
export default {
    props: {
        condition: {
            type: Boolean,
            required: true,
        },
        text: {
            type: String,
            required: true,
        },
        color: {
            type: String,
            required: true,
        },
        onclick: {
            type: Function,
            required: false,
        },
        icon: {
            type: String,
            required: true,
        },
    },
};
</script>

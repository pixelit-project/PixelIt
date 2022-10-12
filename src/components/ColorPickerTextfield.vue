<template>
    <v-text-field v-model="config.clockColor" label="Color" readonly hint="click left on the box">
        <template v-slot:prepend>
            <v-menu v-model="menu" top nudge-bottom="100" nudge-left="16" :close-on-content-click="false">
                <template v-slot:activator="{ on }">
                    <div :style="swatchStyle" class="elevation-1" v-on="on"></div>
                </template>
                <v-card>
                    <v-card-text class="pa-0">
                        <v-color-picker v-model="config.clockColor" />
                    </v-card-text>
                </v-card>
            </v-menu>
        </template>
    </v-text-field>
</template>

<script>
export default {
    data() {
        return {
            menu: false,
        };
    },
    props: {},
    computed: {
        config() {
            return this.$store.state.configData;
        },
        swatchStyle() {
            const { config, menu } = this;
            return {
                backgroundColor: config.clockColor,
                cursor: 'pointer',
                height: '30px',
                width: '30px',
                borderRadius: menu ? '50%' : '4px',
                transition: 'border-radius 200ms ease-in-out',
            };
        },
    },
};
</script>

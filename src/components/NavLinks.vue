<template>
    <v-list>
        <div v-for="item of items" :key="item.title">
            <v-list-item v-if="item.separator == true" disabled :key="item.title" link :to="item.page" :href="item.url" :target="item.target">
                <v-divider></v-divider>
            </v-list-item>
            <v-list-item v-if="item.separator != true" :key="item.title" link :to="item.page" :href="item.url" :target="item.target">
                <v-list-item-icon>
                    <v-icon>{{ item.icon }}</v-icon>
                </v-list-item-icon>

                <v-list-item-content>
                    <v-list-item-title>{{ item.title }}</v-list-item-title>
                </v-list-item-content>
            </v-list-item>
        </div>
    </v-list>
</template>

<script>
export default {
    props: {
        items: {
            type: Array,
            required: true,
        },
    },
};
</script>

<template>
    <v-simple-table>
        <template>
            <tbody>
                <tr v-for="item in items" :key="item.title">
                    <th scope="row" class="name">{{ item.name }}:</th>
                    <td>
                        {{ item.value }}
                    </td>
                </tr>
            </tbody>
        </template>
    </v-simple-table>
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

<style scoped>
.name {
    text-align: left;
    font-weight: 500;
    font-size: 13px !important;
    color: inherit;
}
td, th { padding: 10px 8px !important; height: auto !important; overflow-wrap: anywhere; }
td:last-child { text-align: right; }
</style>

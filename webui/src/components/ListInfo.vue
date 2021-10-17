<template>
    <v-simple-table>
        <template>
            <tbody>
                <tr v-for="item in items" :key="item.title">
                    <td class="name">{{ item.name }}:</td>
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

<style>
.name {
    font-weight: 600;
    font-size: 16px !important;
}
</style>

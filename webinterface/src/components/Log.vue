<template>
    <div class="Log" log-field>
        <v-textarea filled outlined v-model="log" readonly class="log" log-text rows="20"></v-textarea>
    </div>
</template>

<script>
export default {
    name: "Log",
    props: {
        log: {
            type: String,
            required: true,
        },
    },
};
</script>

<style>
[log-text] {
    line-height: 1.4rem !important;
    font-size: 14px;
    color: black;
    resize: none;
}
</style>

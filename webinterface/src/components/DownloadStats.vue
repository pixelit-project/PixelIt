<template>
    <div>
        <v-list v-for="item in items" :key="item.version" class="pa-2">
            <v-list-item-content>
                <v-list-item-title class="pointer" @click="gotToURL(item.downloadURL)">
                    <v-badge color="green" :content="item.downloads" inline>
                        <h3>{{ item.version }} ({{ item.date }})</h3>
                    </v-badge>
                </v-list-item-title>
                <span class="header">Downloads:</span>
                <br />
                <v-list-item-subtitle class="details" v-for="fwdownload in item.fwdownloads" :key="fwdownload.name">
                    &#8226; {{ fwdownload.name }}
                    <span class="downloads">{{ fwdownload.downloads }}</span>
                </v-list-item-subtitle>
                <span class="header">Release notes:</span>
                <br />
                <v-list-item-subtitle class="details" v-for="releaseNote in item.releaseNoteArray" :key="releaseNote"> &#8226; {{ releaseNote }} </v-list-item-subtitle>
            </v-list-item-content>
            <v-divider :inset="false"></v-divider>
        </v-list>
    </div>
</template>

<script>
export default {
    props: {
        items: {
            type: Array,
            required: true,
        },
    },
    methods: {
        gotToURL(url) {
            window.open(url, "_blank");
        },
    },
};
</script>
<style>
.header {
    font-weight: 500;
    font-size: 16px;
    text-indent: 10px;
}
.details {
    text-indent: 20px;
    font-size: 13px;
}
.downloads {
    text-indent: 0px;
    margin-left: 5px;
    font-size: 11px;
    background-color: #4caf50;
    color: white;
    border-radius: 40%;
    height: 13px;
    width: 22px;
    line-height: 13px;
    display: inline-block;
    text-align: center;
}
.smallInfo {
    text-indent: 20px;
    font-size: 13px;
    font-weight: 300;
}
.pointer {
    cursor: pointer;
}
</style>

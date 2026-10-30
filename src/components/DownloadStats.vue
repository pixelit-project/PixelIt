<template>
    <div class="release-history">
        <p v-if="!items.length" class="px-6 pb-4 text--secondary">Release history is currently unavailable.</p>
        <v-expansion-panels v-model="openRelease" flat>
            <v-expansion-panel v-for="item in items" :key="item.version">
                <v-expansion-panel-header class="release-header">
                    <div>
                        <div class="release-title">
                            <strong>{{ item.version }}</strong>
                            <v-chip small label>{{ item.downloads }} downloads</v-chip>
                        </div>
                        <span class="release-date text--secondary">{{ item.date }}</span>
                    </div>
                </v-expansion-panel-header>
                <v-expansion-panel-content>
                    <div class="font-weight-medium mb-2">Release notes</div>
                    <ul class="release-notes mb-4">
                        <li v-for="(note, index) in item.releaseNoteArray" :key="index">{{ note }}</li>
                    </ul>
                    <div class="font-weight-medium mb-2">Downloads by hardware</div>
                    <div v-for="download in item.fwdownloads" :key="download.name" class="download-stat">
                        <span>{{ download.name }}</span><span class="text--secondary">{{ download.downloads }}</span>
                    </div>
                    <v-btn class="mt-4" small text color="primary" :href="item.downloadURL" target="_blank" rel="noopener noreferrer">View release <v-icon right small>mdi-open-in-new</v-icon></v-btn>
                </v-expansion-panel-content>
            </v-expansion-panel>
        </v-expansion-panels>
    </div>
</template>

<script>
export default {
    data() {
        return { openRelease: 0 };
    },
    props: {
        items: { type: Array, required: true },
    },
};
</script>

<style scoped>
.release-header { padding: 16px 24px; }
.release-title { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; }
.release-title strong { font-size: 15px; font-weight: 500; }
.release-date { display: block; margin-top: 6px; font-size: 12px; }
.release-notes { padding-left: 16px; font-size: 13px; line-height: 1.6; overflow-wrap: anywhere; }
.download-stat { display: flex; align-items: baseline; justify-content: space-between; gap: 12px; font-size: 12px; padding: 4px 0; }
.download-stat > span:first-child { overflow-wrap: anywhere; min-width: 0; }
.release-history .v-expansion-panel::before { box-shadow: none; }
</style>

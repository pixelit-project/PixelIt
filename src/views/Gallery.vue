<template>
    <v-container class="gallery app-page">
        <v-sheet v-if="!isLoading" class="gallery-filters" :style="{ top: ($vuetify.application.top + 8) + 'px' }" rounded elevation="2">
            <v-row dense>
                <v-col cols="12" md="6">
                    <v-text-field v-model="message" prepend-inner-icon="mdi-magnify" single-line outlined filled hide-details auto-grow label="Search for name or ID" type="text"></v-text-field>
                </v-col>
                <v-col cols="6" md="3">
                    <v-select v-model="animationFilter" :items="animationFilters" label="Animation" prepend-inner-icon="mdi-animation-play" outlined filled hide-details></v-select>
                </v-col>
                <v-col cols="6" md="3">
                    <v-select v-model="sizeFilter" :items="sizeFilters" label="Size" prepend-inner-icon="mdi-image-size-select-large" outlined filled hide-details></v-select>
                </v-col>
            </v-row>
        </v-sheet>
        <v-row>
            <v-col v-for="n in (isLoading ? 6 : 0)" :key="'loading-' + n" cols="6" md="4" lg="2">
                <v-skeleton-loader type="image, article" aria-label="Loading gallery" />
            </v-col>
            <v-col v-if="loadError" cols="12" class="text-center">
                Could not load the gallery.
                <v-btn text @click="loadBMPs">Retry</v-btn>
            </v-col>
            <v-col v-for="bmp in visibleBMPs" :key="filterRevision + ':' + bmp.id" cols="6" lg="2">
                <GalleryViewportItem :pinned="openBitmapId === bmp.id">
                    <v-card class="ui-panel gallery-card" elevation="2">
                        <div class="text-center">
                            {{ bmp.name }}
                        </div>
                        <v-divider class="my-4" />
                        <BmpCanvas :bmp="bmp" />
                        <p></p>
                        <div class="text-center">
                            <BmpDialog :bmp="bmp" @dialog-change="openBitmapId = $event ? bmp.id : null" />
                            <v-btn icon @click="sendBitmap(bmp.rgB565Array, bmp.sizeX)" :disabled="!sockedIsConnected" class="float-right" title="Show on PixelIt" aria-label="Show on PixelIt">
                                <v-icon>mdi-arrow-right-circle-outline</v-icon>
                            </v-btn>
                        </div>
                        <div class="text-center">
                            <v-text-field prepend-inner-icon="mdi-identifier" rounded dense hide-details readonly :value="bmp.id" append-outer-icon="mdi-content-copy" @click:append-outer="copyText(bmp.id, $event)"></v-text-field>
                        </div>
                    </v-card>
                </GalleryViewportItem>
            </v-col>
            <v-col v-if="!isLoading && !loadError && getBMPs.length === 0" cols="12" class="text-center">
                No matching bitmaps found.
            </v-col>
            <v-col v-show="hasMoreBMPs" cols="12" class="text-center">
                <div ref="loadMoreTrigger">
                    <v-row v-if="automaticLoading" aria-hidden="true">
                        <v-col v-for="placeholder in placeholderCount" :key="placeholder" cols="6" lg="2">
                            <div class="gallery-placeholder">
                                <div class="placeholder-title"></div>
                                <div class="placeholder-preview"></div>
                                <div class="placeholder-actions"></div>
                            </div>
                        </v-col>
                    </v-row>
                    <v-btn v-else text @click="showMoreBMPs">Load more</v-btn>
                </div>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
import actionFeedback from '../mixins/actionFeedback';
import { notify } from '../utils/feedback';
import BmpCanvas from '../components/BmpCanvas';
import BmpDialog from '../components/BmpDialog';
import GalleryViewportItem from '../components/GalleryViewportItem';

const BATCH_SIZE = 24;
let pendingBMPsRequest = null;
export default {
    mixins: [actionFeedback],
    feedbackActions: { sendBitmap: 'Bitmap sent to PixelIt.' },
    name: 'Gallery',
    created: function () {
        this.loadBMPs();
    },
    mounted() {
        if (typeof IntersectionObserver !== 'undefined') {
            this.automaticLoading = true;
            this.loadMoreObserver = new IntersectionObserver((entries) => {
                if (entries.some((entry) => entry.isIntersecting)) {
                    this.showMoreBMPs();
                }
            }, { rootMargin: '0px 0px 1800px 0px' });
            this.observeLoadMore();
        }
    },
    beforeDestroy() {
        if (this.loadMoreObserver) this.loadMoreObserver.disconnect();
    },
    data: function () {
        return {
            message: '',
            filterRevision: 0,
            animationFilter: 'all',
            sizeFilter: 'all',
            animationFilters: [
                { text: 'All bitmaps', value: 'all' },
                { text: 'Animated', value: 'animated' },
                { text: 'Not animated', value: 'static' },
            ],
            openBitmapId: null,
            visibleCount: BATCH_SIZE,
            automaticLoading: false,
            isLoading: false,
            loadError: false,
        };
    },
    components: {
        BmpCanvas,
        BmpDialog,
        GalleryViewportItem,
    },
    watch: {
        message() {
            this.resetResults();
        },
        animationFilter() {
            this.resetResults();
        },
        sizeFilter() {
            this.resetResults();
        },
        visibleBMPs() {
            // Recheck after each batch, even when the trigger stays in a tall viewport.
            this.$nextTick(this.observeLoadMore);
        },
    },
    methods: {
        resetResults() {
            this.visibleCount = BATCH_SIZE;
            // Drop retained viewport state when a bitmap moves within filtered results.
            this.filterRevision++;
            if (!this.$el || typeof window === 'undefined') return;
            const top = Math.max(0, this.$el.getBoundingClientRect().top + window.scrollY
                - this.$vuetify.application.top - 8);
            if (window.scrollY > top) {
                // Move before Vue patches the shorter list; smooth scrolling would
                // temporarily leave the viewport below the new results.
                window.scrollTo({ top, behavior: 'instant' });
            }
        },
        observeLoadMore() {
            if (!this.loadMoreObserver || this._isBeingDestroyed || this._isDestroyed) return;
            this.loadMoreObserver.disconnect();
            if (this.hasMoreBMPs && this.$refs.loadMoreTrigger) {
                this.loadMoreObserver.observe(this.$refs.loadMoreTrigger);
            }
        },
        showMoreBMPs() {
            this.visibleCount = Math.min(this.visibleCount + BATCH_SIZE, this.getBMPs.length);
        },
        async loadBMPs() {
            if (this.$store.state.bmpsFromAPI.length) return;
            this.isLoading = true;
            this.loadError = false;
            try {
                this.$store.state.bmpsFromAPI = await getBMPsFromAPI();
            } catch (error) {
                console.error('Could not load gallery', error);
                this.loadError = true;
            } finally {
                this.isLoading = false;
            }
        },
        async copyText(value) {
            try {
                await navigator.clipboard.writeText(String(value));
                notify('Bitmap ID copied.', 'success');
            } catch (error) { notify('Could not copy the ID. Select and copy it manually.', 'error'); }
        },
        sendBitmap(rgB565Array, sizeX) {
            if (rgB565Array.endsWith(',')) {
                rgB565Array = rgB565Array.slice(0, -1);
            }
            const frames = JSON.parse(`[${rgB565Array}]`);
            if (sizeX == 8 || frames.length > 1) {
                this.$socket.sendObj({
                    setScreen: {
                        bitmapAnimation: {
                            data: frames,
                            animationDelay: 200,
                            size: { width: Number(sizeX), height: 8 },
                            position: { x: 0, y: 0 },
                        },
                    },
                });
            } else {
                this.$socket.sendObj({                   
                    setScreen: {
                        bitmap: {
                            data: JSON.parse(rgB565Array),
                            position: {
                                x: 0,
                                y: 0,
                            },
                            size: {
                                width: sizeX,
                                height: 8,
                            },
                        },
                    },
                });
            }
        },
    },
    computed: {
        sizeFilters() {
            const sizes = new Map();
            this.$store.state.bmpsFromAPI.forEach((bmp) => {
                const width = Number(bmp.sizeX);
                const height = Number(bmp.sizeY);
                if (Number.isInteger(width) && width > 0 && Number.isInteger(height) && height > 0) {
                    sizes.set(`${width}x${height}`, { width, height });
                }
            });
            return [{ text: 'All sizes', value: 'all' }, ...Array.from(sizes.values())
                .sort((a, b) => a.width - b.width || a.height - b.height)
                .map(({ width, height }) => ({ text: `${width} × ${height}`, value: `${width}x${height}` }))];
        },
        getBMPs() {
            const bmps = this.$store.state.bmpsFromAPI;
            if (!this.message && this.animationFilter === 'all' && this.sizeFilter === 'all') return bmps;
            const query = this.message.toLowerCase();
            return bmps.filter((bmp) => {
                const matchesSearch = !query || bmp.name.toLowerCase().includes(query) || bmp.id == this.message;
                const matchesAnimation = this.animationFilter === 'all'
                    || Boolean(bmp.animated) === (this.animationFilter === 'animated');
                const matchesSize = this.sizeFilter === 'all'
                    || `${Number(bmp.sizeX)}x${Number(bmp.sizeY)}` === this.sizeFilter;
                return matchesSearch && matchesAnimation && matchesSize;
            });
        },
        hasMoreBMPs() {
            return this.visibleCount < this.getBMPs.length;
        },
        placeholderCount() {
            return Math.max(0, Math.min(6, this.getBMPs.length - this.visibleCount));
        },
        visibleBMPs() {
            return this.getBMPs.slice(0, this.visibleCount);
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
    },
};

function getBMPsFromAPI() {
    if (!pendingBMPsRequest) {
        pendingBMPsRequest = fetch('https://pixelit.bastelbunker.de/api/GetBMPAll')
            .then((response) => {
                if (!response.ok) throw new Error(`HTTP ${response.status}`);
                return response.json();
            })
            // Gallery records are read-only. Avoid observing every bitmap field in Vue.
            .then((bmps) => Object.freeze(bmps.map((bmp) => Object.freeze(bmp))))
            .finally(() => {
                pendingBMPsRequest = null;
            });
    }
    return pendingBMPsRequest;
}
</script>

<style scoped>
.gallery {
    overflow-anchor: none;
}

.gallery-filters {
    position: sticky;
    z-index: 4;
    padding: 12px;
    margin-bottom: 20px;
}

.gallery-card {
    animation: gallery-appear 160ms ease-out;
}
.gallery-placeholder {
    height: 300px;
    padding: 12px;
    border-radius: 4px;
    background: rgba(128, 128, 128, 0.08);
}
.placeholder-title,
.placeholder-preview,
.placeholder-actions {
    background: rgba(128, 128, 128, 0.14);
    border-radius: 4px;
}
.placeholder-title {
    width: 70%;
    height: 20px;
    margin: 0 auto 24px;
}
.placeholder-preview {
    height: 150px;
    margin-bottom: 24px;
}
.placeholder-actions {
    height: 36px;
}
@keyframes gallery-appear {
    from { opacity: 0; }
    to { opacity: 1; }
}
@media (prefers-reduced-motion: reduce) {
    .gallery-card {
        animation: none;
    }
}

canvas {
    padding-left: 0;
    padding-right: 0;
    margin-left: auto;
    margin-right: auto;
    display: block;
    border: 1px solid;
    border-color: grey;
}
</style>

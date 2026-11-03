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
            <div class="gallery-compatibility-filter">
                <v-checkbox v-model="showIncompatible" :disabled="!deviceLimits" hide-details dense
                    label="Also show images incompatible with this device" class="ma-0 pa-0" />
                <span v-if="!deviceLimits" class="text-caption" role="status">
                    {{ sockedIsConnected ? 'Waiting for device information.' : 'Connect a device to check compatibility. Browsing and previews are available.' }}
                </span>
                <span v-else-if="!showIncompatible && hiddenCount" class="text-caption" role="status"
                    title="Enable the checkbox to view images incompatible with this device.">
                    {{ hiddenCount }} incompatible hidden
                </span>
            </div>
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
                <GalleryViewportItem :pinned="openBitmapId === bmp.id || openReportBitmapId === bmp.id">
                    <v-card class="ui-panel gallery-card" elevation="2">
                        <div class="gallery-title" :title="bmp.name">{{ bmp.name }}</div>
                        <div class="gallery-preview">
                            <BmpCanvas v-if="metadata(bmp).valid" :bmp="bmp" />
                        </div>
                        <p v-if="metadata(bmp).valid" class="gallery-summary">{{ formatSummary(bmp) }}</p>
                        <details v-if="!metadata(bmp).valid || (deviceLimits && !compatibility(bmp).ok)" class="gallery-compatibility" :class="{ 'gallery-compatibility-invalid': !metadata(bmp).valid }">
                            <summary :title="compatibility(bmp).reason">
                                <v-icon small aria-hidden="true">{{ metadata(bmp).valid ? 'mdi-alert-outline' : 'mdi-alert-circle-outline' }}</v-icon>
                                {{ metadata(bmp).valid ? 'Not compatible' : 'Invalid image' }}
                            </summary>
                            <p class="gallery-compatibility-reason">{{ compatibility(bmp).reason }}</p>
                        </details>
                        <div class="gallery-footer">
                            <v-btn text small class="gallery-copy" @click="copyText(bmp.id)" :title="'Copy ID ' + bmp.id" :aria-label="'Copy ID ' + bmp.id">
                                <span>#{{ bmp.id }}</span><v-icon small right>mdi-content-copy</v-icon>
                            </v-btn>
                            <div class="gallery-report">
                                <ButtonBMPReport compact :bmp="bmp" @dialog-change="openReportBitmapId = $event ? bmp.id : null" />
                            </div>
                            <div class="gallery-actions">
                                <BmpDialog :bmp="bmp" @dialog-change="openBitmapId = $event ? bmp.id : null" />
                                <v-btn icon color="primary" @click="sendBitmap(bmp)" :disabled="!sockedIsConnected || !compatibility(bmp).ok" :title="sendLabel(bmp)" :aria-label="sendLabel(bmp)">
                                    <v-icon>mdi-arrow-right-circle-outline</v-icon>
                                </v-btn>
                            </div>
                        </div>
                    </v-card>
                </GalleryViewportItem>
            </v-col>
            <v-col v-if="!isLoading && !loadError && getBMPs.length === 0" cols="12" class="text-center">
                {{ hiddenCount && !showIncompatible ? 'No compatible images match your search. Enable incompatible images above to browse all matches.' : 'No matching bitmaps found.' }}
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
import { galleryDeviceLimits, galleryMetadata, galleryCompatibility, buildGalleryCommand } from '../utils/galleryCompatibility';
import { notify } from '../utils/feedback';
import { copyToClipboard } from '../utils/clipboard';
import BmpCanvas from '../components/BmpCanvas';
import BmpDialog from '../components/BmpDialog';
import ButtonBMPReport from '../components/ButtonBMPReport';
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
            showIncompatible: false,
            filterRevision: 0,
            animationFilter: 'all',
            sizeFilter: 'all',
            animationFilters: [
                { text: 'All bitmaps', value: 'all' },
                { text: 'Animated', value: 'animated' },
                { text: 'Not animated', value: 'static' },
            ],
            openBitmapId: null,
            openReportBitmapId: null,
            visibleCount: BATCH_SIZE,
            automaticLoading: false,
            isLoading: false,
            loadError: false,
        };
    },
    components: {
        BmpCanvas,
        BmpDialog,
        ButtonBMPReport,
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
        showIncompatible() { this.resetResults(); },
        compatibilityKey() { this.resetResults(); },
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
                this.$store.state.bmpsFromAPI = await getBMPsFromAPI(this.$apiServerBaseURL);
            } catch (error) {
                console.error('Could not load gallery', error);
                this.loadError = true;
            } finally {
                this.isLoading = false;
            }
        },
        async copyText(value) {
            try {
                await copyToClipboard(value);
                notify('Bitmap ID copied.', 'success');
            } catch (error) { notify('Could not copy the ID. Select and copy it manually.', 'error'); }
        },
        metadata: galleryMetadata,
        compatibility(bmp) { return galleryCompatibility(bmp, this.deviceLimits); },
        formatSummary(bmp) {
            const meta = galleryMetadata(bmp);
            return meta.valid ? `${meta.width}×${meta.height} · ${meta.frames} ${meta.frames === 1 ? 'frame' : 'frames'}` : 'Invalid bitmap format';
        },
        sendLabel(bmp) { return this.compatibility(bmp).ok ? 'Show on PixelIt' : this.compatibility(bmp).reason; },
        sendBitmap(bmp) {
            if (!this.sockedIsConnected || !this.$socket || this.$socket.readyState !== 1) {
                throw new Error('No device connection. Reconnect your PixelIt and try again.');
            }
            // Re-read capabilities at the click, including after a reconnect.
            const command = buildGalleryCommand(bmp, this.deviceLimits);
            this.$socket.sendObj(command);
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
        deviceLimits() { return galleryDeviceLimits(this.$store.state); },
        compatibilityKey() { return JSON.stringify(this.deviceLimits); },
        hiddenCount() {
            return this.deviceLimits ? this.matchingBMPs.filter(bmp => !galleryCompatibility(bmp, this.deviceLimits).ok).length : 0;
        },
        getBMPs() {
            return this.deviceLimits && !this.showIncompatible ? this.matchingBMPs.filter(bmp => galleryCompatibility(bmp, this.deviceLimits).ok) : this.matchingBMPs;
        },
        matchingBMPs() {
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

function getBMPsFromAPI(baseURL) {
    if (!pendingBMPsRequest) {
        pendingBMPsRequest = fetch(`${baseURL.replace(/\/$/, '')}/GetBMPAll`)
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

.gallery-compatibility-filter {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 2px 16px;
    padding-top: 4px;
}
.gallery-compatibility-filter ::v-deep .v-input__slot {
    margin-bottom: 0;
}
.gallery-compatibility-filter ::v-deep .v-label {
    font-size: 12px;
}
.gallery-compatibility-filter > .text-caption {
    opacity: .65;
}
.gallery-card {
    container-type: inline-size;
    container-name: gallery-card;
    animation: gallery-appear 160ms ease-out;
    display: flex;
    flex-direction: column;
    gap: 12px;
}
.gallery-title {
    text-align: center;
    font-size: 14px;
    line-height: 20px;
    height: 20px;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
}
.gallery-preview {
    height: 150px;
    display: flex;
    align-items: center;
    justify-content: center;
    min-width: 0;
}
.gallery-preview canvas {
    max-width: 100%;
    max-height: 150px;
    object-fit: contain;
    image-rendering: pixelated;
}
.gallery-summary {
    text-align: center;
    font-size: 12px;
    opacity: .65;
    margin: 0;
}
.gallery-footer {
    margin: auto -16px 0;
    padding-top: 4px;
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto minmax(0, 1fr);
    align-items: center;
}
@media (max-width: 599px) {
    .gallery-footer { margin-left: -8px; margin-right: -8px; }
}
@container gallery-card (max-width: 160px) {
    .gallery-footer {
        grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
        row-gap: 4px;
    }
    .gallery-copy.v-btn {
        grid-column: 1;
        grid-row: 1;
    }
    .gallery-actions {
        grid-column: 2;
        grid-row: 1;
    }
    .gallery-report {
        grid-column: 1 / -1;
        grid-row: 2;
        justify-self: center;
    }
}
.gallery-copy.v-btn {
    justify-self: start;
    text-transform: none;
    font-size: 11px;
    opacity: .65;
    min-width: 0;
    padding: 0 6px;
}
.gallery-actions {
    justify-self: end;
    display: flex;
    align-items: center;
    justify-content: center;
}
.gallery-actions ::v-deep .v-btn {
    float: none !important;
    width: 28px;
    height: 32px;
}
.gallery-footer ::v-deep .gallery-report-button {
    width: auto;
    min-width: 0;
    height: 26px;
    padding: 0 6px;
    margin: 0 3px;
    font-size: 10px;
    text-transform: none;
    letter-spacing: normal;
    color: #37759f;
    border-color: rgba(65, 135, 185, .35);
    background: rgba(65, 135, 185, .06);
}
.theme--dark .gallery-footer ::v-deep .gallery-report-button {
    color: #87b1ce;
}
.gallery-compatibility {
    text-align: center;
    font-size: 12px;
    overflow-wrap: anywhere;
}
.gallery-compatibility summary {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 4px 10px;
    border-radius: 12px;
    background: rgba(180, 120, 30, .08);
    color: inherit;
    border: 1px solid rgba(180, 120, 30, .4);
    font-weight: 500;
    cursor: pointer;
    list-style: none;
}
.gallery-compatibility-invalid summary {
    background: rgba(190, 80, 70, .08);
    border-color: rgba(190, 80, 70, .4);
}
.gallery-compatibility summary .v-icon {
    color: inherit;
}
.gallery-compatibility summary::-webkit-details-marker {
    display: none;
}
.gallery-compatibility summary:hover,
.gallery-compatibility summary:focus-visible,
.gallery-compatibility[open] summary {
    opacity: 1;
}
.gallery-compatibility summary:focus-visible {
    outline: 2px solid currentColor;
    outline-offset: 2px;
}
.gallery-compatibility-reason {
    margin: 6px 0 0;
    opacity: .75;
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

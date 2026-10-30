<template>
    <v-container class="gallery app-page">
        <v-row>
            <v-col cols="12" lg="12">
                <v-bottom-navigation :value="pixelMode" color="primary">
                    <v-btn @click="changePixelModeTo8x8()">
                        <span>8x8 Pixel</span>
                        <v-icon>mdi-grid</v-icon>
                    </v-btn>

                    <v-btn @click="changePixelModeTo8x32()">
                        <span>8x32 Pixel</span>
                        <v-icon>mdi-grid</v-icon>
                    </v-btn>
                </v-bottom-navigation>
            </v-col>
        </v-row>
        <v-row v-if="pixelMode == 0">
            <v-col cols="12" lg="4" offset-lg="2">
                <v-card class="ui-panel" elevation="2">
                    <div class="editor-tools mb-3">
                        <v-btn-toggle v-model="tool" mandatory dense color="primary">
                            <v-btn value="pen" aria-label="Pencil" title="Pencil"><v-icon small>mdi-pencil</v-icon></v-btn>
                            <v-btn value="eraser" aria-label="Eraser" title="Eraser"><v-icon small>mdi-eraser</v-icon></v-btn>
                            <v-btn value="fill" aria-label="Flood fill" title="Flood fill"><v-icon small>mdi-format-color-fill</v-icon></v-btn>
                            <v-btn value="picker" aria-label="Pick color" title="Pick color"><v-icon small>mdi-eyedropper</v-icon></v-btn>
                        </v-btn-toggle>
                        <div>
                            <v-btn icon small :disabled="!undoStack.length" aria-label="Undo" title="Undo" @click="undo"><v-icon>mdi-undo</v-icon></v-btn>
                            <v-btn icon small :disabled="!redoStack.length" aria-label="Redo" title="Redo" @click="redo"><v-icon>mdi-redo</v-icon></v-btn>
                        </div>
                    </div>
                    <div class="pixel-editor" role="group" aria-label="8 by 8 pixel editor">
                        <button v-for="(pixel, index) in frames[selectedFrame]" :key="index" type="button" class="editor-pixel"
                            :style="{ backgroundColor: pixelColor(pixel) }" :aria-label="'Pixel ' + (index + 1)" @click="paintPixel(index)">
                            <span v-if="onionSkin && selectedFrame > 0 && pixel === 0" class="onion-pixel"
                                :style="{ backgroundColor: pixelColor(frames[selectedFrame - 1][index]) }"></span>
                        </button>
                    </div>
                    <div class="editor-options">
                        <v-switch v-model="onionSkin" label="Previous frame overlay" :disabled="selectedFrame === 0" dense hide-details class="mt-0"></v-switch>
                        <v-btn small text :disabled="!!animationError || !frames[selectedFrame].some(pixel => pixel !== 0)" @click="clearFrame">Clear frame</v-btn>
                    </div>
                    <div class="text-caption text--secondary mt-2" role="status">{{ draftStatus }}</div>
                    <section class="animation-studio mt-5">
                        <div class="studio-heading">
                            <div>
                                <div class="text-overline primary--text">ANIMATION</div>
                                <div class="text-subtitle-1 font-weight-bold">Bring your pixels to life</div>
                            </div>
                            <v-chip small outlined>{{ frames.length }} {{ frames.length === 1 ? 'frame' : 'frames' }}</v-chip>
                        </div>
                        <div class="frame-list" :class="{ 'frame-list-dragging': draggedFrame !== null }" role="group" aria-label="Animation frames">
                            <button v-for="(frame, index) in frames" :key="index" type="button"
                                class="frame-tile" :class="{ 'frame-tile-active': selectedFrame === index, 'frame-dragging': draggedFrame === index,
                                    'frame-reorderable': !animationError && frames.length > 1, 'frame-drop-before': dropPosition === index, 'frame-drop-after': dropPosition === frames.length && index === frames.length - 1 }"
                                :draggable="false" :data-frame-index="index"
                                @dragstart.prevent @pointerdown="startFrameDrag($event, index)"
                                @pointermove="overFrame" @pointerup="dropFrame"
                                @pointercancel="endFrameDrag" @lostpointercapture="endFrameDrag"
                                :aria-label="'Edit frame ' + (index + 1)" :aria-pressed="selectedFrame === index" @click="selectFrameFromClick(index)">
                                <span class="frame-thumbnail">
                                    <span v-for="(pixel, pixelIndex) in frame" :key="pixelIndex" :style="{ backgroundColor: pixelColor(pixel) }"></span>
                                </span>
                                <span class="frame-caption">{{ String(index + 1).padStart(2, '0') }}</span>
                            </button>
                            <button type="button" class="frame-tile frame-add" aria-label="Add empty frame" :disabled="!!animationError" @click="addFrame(false)">
                                <v-icon>mdi-plus</v-icon>
                                <span class="frame-caption">New</span>
                            </button>
                        </div>
                        <div v-if="draggedFrame !== null" class="frame-tile frame-drag-preview" aria-hidden="true"
                            :style="{ left: frameDragPosition.x + 'px', top: frameDragPosition.y + 'px' }">
                            <span class="frame-thumbnail">
                                <span v-for="(pixel, pixelIndex) in frames[draggedFrame]" :key="pixelIndex" :style="{ backgroundColor: pixelColor(pixel) }"></span>
                            </span>
                            <span class="frame-caption">{{ String(draggedFrame + 1).padStart(2, '0') }}</span>
                        </div>
                        <div class="frame-toolbar">
                            <span class="text-caption text--secondary">Editing frame {{ selectedFrame + 1 }}</span>
                            <div class="frame-actions">
                                <v-btn icon small :disabled="selectedFrame === 0 || !!animationError" aria-label="Move selected frame left" title="Move frame left" @click="moveFrame(-1)"><v-icon small>mdi-arrow-left</v-icon></v-btn>
                                <v-btn icon small :disabled="selectedFrame === frames.length - 1 || !!animationError" aria-label="Move selected frame right" title="Move frame right" @click="moveFrame(1)"><v-icon small>mdi-arrow-right</v-icon></v-btn>
                                <v-btn small text :disabled="!!animationError" @click="addFrame(true)"><v-icon small left>mdi-content-copy</v-icon>Duplicate</v-btn>
                                <v-btn icon small :disabled="frames.length === 1 || !!animationError" aria-label="Delete selected frame" title="Delete selected frame" @click="deleteFrame"><v-icon small>mdi-trash-can-outline</v-icon></v-btn>
                            </div>
                        </div>
                        <div class="preview-panel">
                            <div class="animation-preview" aria-label="Animation preview">
                                <div v-for="(color, index) in previewBackground" :key="index" :style="{ backgroundColor: color }"></div>
                            </div>
                            <div class="preview-controls">
                                <div class="text-subtitle-2">{{ playing ? 'Playing animation' : 'Animation preview' }}</div>
                                <div class="text-caption text--secondary mb-3">{{ frames.length > 1 ? (1000 / previewDelay).toFixed(1) + ' fps · Loop' : 'Add a frame to start animating' }}</div>
                                <v-btn small rounded depressed color="primary" :disabled="frames.length < 2 || !!animationError" @click="togglePreview">
                                    <v-icon small left>{{ playing ? 'mdi-pause' : 'mdi-play' }}</v-icon>
                                    {{ playing ? 'Pause' : 'Play animation' }}
                                </v-btn>
                            </div>
                        </div>
                        <v-slider v-model="previewDelay" min="50" max="1000" step="50" label="Frame delay" :hint="previewDelay + ' ms · Preview and Live draw only'" persistent-hint class="mt-4" @change="restartPreview"></v-slider>
                    </section>
                    <v-expansion-panels flat class="mt-3">
                        <v-expansion-panel>
                            <v-expansion-panel-header class="px-2 text-caption">RGB565 data</v-expansion-panel-header>
                            <v-expansion-panel-content>
                                <v-textarea outlined v-model="array8x8String" label="RGB565 frames" :error-messages="animationError" rows="4" class="bitmap-data"></v-textarea>
                            </v-expansion-panel-content>
                        </v-expansion-panel>
                    </v-expansion-panels>
                    <v-alert v-if="animationError" dense text type="error" class="mt-2">{{ animationError }}</v-alert>
                    <div class="creator-footer">
                        <v-switch class="mt-0 pt-0" v-model="livedraw" label="Live draw" :disabled="!sockedIsConnected || !!animationError" hide-details dense></v-switch>
                        <ButtonSave color="green" text="Save" icon="mdi-content-save" :data="array8x8String" :pixelMode="pixelMode" :condition="!animationError"></ButtonSave>
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="ui-panel" elevation="2">
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="280"></v-color-picker>
                </v-card>
            </v-col>
        </v-row>
        <v-row v-if="pixelMode == 1">
            <v-col cols="12" lg="8" offset-lg="0">
                <v-card class="ui-panel" elevation="2">
                    <div class="editor-tools mb-3">
                        <v-btn-toggle v-model="tool" mandatory dense color="primary">
                            <v-btn value="pen" aria-label="Pencil" title="Pencil"><v-icon small>mdi-pencil</v-icon></v-btn>
                            <v-btn value="eraser" aria-label="Eraser" title="Eraser"><v-icon small>mdi-eraser</v-icon></v-btn>
                            <v-btn value="fill" aria-label="Flood fill" title="Flood fill"><v-icon small>mdi-format-color-fill</v-icon></v-btn>
                            <v-btn value="picker" aria-label="Pick color" title="Pick color"><v-icon small>mdi-eyedropper</v-icon></v-btn>
                        </v-btn-toggle>
                        <div>
                            <v-btn icon small :disabled="!undoStack32.length" aria-label="Undo" title="Undo" @click="undo32"><v-icon>mdi-undo</v-icon></v-btn>
                            <v-btn icon small :disabled="!redoStack32.length" aria-label="Redo" title="Redo" @click="redo32"><v-icon>mdi-redo</v-icon></v-btn>
                        </div>
                    </div>
                    <div class="wide-editor-scroll">
                        <div class="pixel-editor pixel-editor-wide" role="group" aria-label="32 by 8 pixel editor">
                            <button v-for="(pixel, index) in bitmap32" :key="index" type="button" class="editor-pixel"
                                :style="{ backgroundColor: pixelColor(pixel) }" :aria-label="'Pixel ' + (index + 1)" @click="paintPixel(index)"></button>
                        </div>
                    </div>
                    <div class="editor-options">
                        <span class="text-caption text--secondary">32 × 8 pixels</span>
                        <v-btn small text :disabled="!!bitmap32Error || !bitmap32.some(pixel => pixel !== 0)" @click="clearBitmap32">Clear image</v-btn>
                    </div>
                    <div class="text-caption text--secondary mt-2" role="status">{{ draft32Status }}</div>
                    <v-expansion-panels flat class="mt-3">
                        <v-expansion-panel>
                            <v-expansion-panel-header class="px-2 text-caption">RGB565 data</v-expansion-panel-header>
                            <v-expansion-panel-content>
                                <v-textarea outlined v-model="array8x32String" label="RGB565 bitmap" :error-messages="bitmap32Error" rows="5" class="bitmap-data"></v-textarea>
                            </v-expansion-panel-content>
                        </v-expansion-panel>
                    </v-expansion-panels>
                    <v-alert v-if="bitmap32Error" dense text type="error" class="mt-2">{{ bitmap32Error }}</v-alert>
                    <div class="creator-footer">
                        <v-switch class="mt-0 pt-0" v-model="livedraw" label="Live draw" :disabled="!sockedIsConnected || !!bitmap32Error" hide-details dense></v-switch>
                        <ButtonSave color="green" text="Save" icon="mdi-content-save" :data="array8x32String" :pixelMode="pixelMode" :condition="!bitmap32Error"></ButtonSave>
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="ui-panel" elevation="2">
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="270"></v-color-picker>
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>
<script>
import ButtonSave from '../components/ButtonBMPSave';
export default {
    data() {
        return {
            colors: '#F44336',
            pixelMode: 0,
            active8x8Background: Array.from({ length: 64 }, () => '#000000'),
            bitmap32: Array(256).fill(0),
            bitmap32Error: '',
            undoStack32: [],
            redoStack32: [],
            draft32Status: 'Draft saved automatically in this browser',
            array8x8String: `[${Array.from({ length: 64 }, () => 0).join(',')}]`,
            array8x32String: `[${Array.from({ length: 256 }, () => 0).join(',')}]`,
            livedraw: false,
            frames: [Array(64).fill(0)],
            selectedFrame: 0,
            previewFrame: 0,
            playing: false,
            previewTimer: null,
            animationError: '',
            draggedFrame: null,
            framePointer: null,
            frameDragPosition: { x: 0, y: 0 },
            suppressFrameClick: false,
            dropPosition: null,
            tool: 'pen',
            onionSkin: false,
            previewDelay: 200,
            livePreview: false,
            undoStack: [],
            redoStack: [],
            draftStatus: 'Draft saved automatically in this browser',
        };
    },
    components: {
        ButtonSave,
    },
    mounted() {
        this.restoreDraft();
        this.restoreDraft32();
    },
    beforeDestroy() {
        this.endFrameDrag();
        this.stopPreview();
    },
    computed: {
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        previewBackground() {
            return this.frames[this.playing ? this.previewFrame : this.selectedFrame].map(rgb565ToHex);
        },
    },
    watch: {
        onionSkin() { this.saveDraft(); },
        previewDelay() { this.saveDraft(); },
        array8x8String(value) {
            try {
                const parsed = JSON.parse(`[${value.trim().replace(/,$/, '')}]`);
                if (!validFrames(parsed)) {
                    throw new Error('Invalid frames');
                }
                if (JSON.stringify(parsed) !== JSON.stringify(this.frames)) {
                    this.recordHistory();
                    this.stopPreview();
                    this.frames = parsed;
                    this.selectedFrame = Math.min(this.selectedFrame, parsed.length - 1);
                    this.active8x8Background = parsed[this.selectedFrame].map(rgb565ToHex);
                }
                this.saveDraft();
                this.animationError = '';
            } catch (error) {
                this.stopPreview();
                this.animationError = 'Enter frames of 64 RGB565 values (0–65535), separated by commas.';
            }
        },
        array8x32String(value) {
            try {
                const parsed = JSON.parse(value.trim().replace(/,$/, ''));
                if (!validBitmap(parsed, 256)) throw new Error('Invalid bitmap');
                if (JSON.stringify(parsed) !== JSON.stringify(this.bitmap32)) {
                    this.recordHistory32();
                    this.bitmap32 = parsed;
                }
                this.bitmap32Error = '';
                this.saveDraft32();
            } catch (error) {
                this.bitmap32Error = 'Enter one bitmap of 256 RGB565 values (0–65535). Animations are not supported for 8×32.';
            }
        },
    },
    methods: {
        pixelColor: rgb565ToHex,
        recordHistory32() {
            this.undoStack32.push(this.bitmap32.slice());
            if (this.undoStack32.length > 100) this.undoStack32.shift();
            this.redoStack32 = [];
        },
        undo32() {
            if (!this.undoStack32.length) return;
            this.redoStack32.push(this.bitmap32.slice());
            this.bitmap32 = this.undoStack32.pop();
            this.bitmap32Error = '';
            this.commitBitmap32();
        },
        redo32() {
            if (!this.redoStack32.length) return;
            this.undoStack32.push(this.bitmap32.slice());
            this.bitmap32 = this.redoStack32.pop();
            this.bitmap32Error = '';
            this.commitBitmap32();
        },
        commitBitmap32() {
            this.array8x32String = JSON.stringify(this.bitmap32);
            this.saveDraft32();
            if (this.livedraw && this.sockedIsConnected && this.pixelMode === 1 && !this.bitmap32Error) {
                this.$socket.sendObj({ setScreen: { bitmap: {
                    data: this.bitmap32.slice(), position: { x: 0, y: 0 }, size: { width: 32, height: 8 },
                } } });
            }
        },
        clearBitmap32() {
            if (this.bitmap32Error || !this.bitmap32.some(pixel => pixel !== 0)) return;
            this.recordHistory32();
            this.bitmap32 = Array(256).fill(0);
            this.commitBitmap32();
        },
        saveDraft32() {
            try {
                localStorage.setItem('pixelit-creator-8x32-v1', JSON.stringify(this.bitmap32));
                this.draft32Status = 'Draft saved in this browser';
            } catch (error) {
                this.draft32Status = 'Draft could not be saved. Browser storage is unavailable or full.';
            }
        },
        restoreDraft32() {
            try {
                const value = localStorage.getItem('pixelit-creator-8x32-v1');
                if (!value) return;
                const bitmap = JSON.parse(value);
                if (!validBitmap(bitmap, 256)) throw new Error('Invalid draft');
                this.bitmap32 = bitmap;
                this.array8x32String = JSON.stringify(bitmap);
                this.draft32Status = 'Draft restored · Changes saved automatically';
            } catch (error) {
                this.draft32Status = 'Saved draft could not be restored.';
            }
        },
        snapshot() {
            return { frames: this.frames.map(frame => frame.slice()), selectedFrame: this.selectedFrame };
        },
        recordHistory() {
            this.undoStack.push(this.snapshot());
            if (this.undoStack.length > 100) this.undoStack.shift();
            this.redoStack = [];
        },
        restoreSnapshot(snapshot) {
            this.stopPreview();
            this.frames = snapshot.frames.map(frame => frame.slice());
            this.selectedFrame = snapshot.selectedFrame;
            this.animationError = '';
            this.active8x8Background = this.frames[this.selectedFrame].map(rgb565ToHex);
            this.serializeFrames();
            this.sendLiveFrame(this.selectedFrame);
        },
        undo() {
            if (!this.undoStack.length) return;
            this.redoStack.push(this.snapshot());
            this.restoreSnapshot(this.undoStack.pop());
        },
        redo() {
            if (!this.redoStack.length) return;
            this.undoStack.push(this.snapshot());
            this.restoreSnapshot(this.redoStack.pop());
        },
        saveDraft() {
            try {
                localStorage.setItem('pixelit-creator-8x8-v1', JSON.stringify({
                    ...this.snapshot(), previewDelay: this.previewDelay, onionSkin: this.onionSkin,
                }));
                this.draftStatus = 'Draft saved in this browser';
            } catch (error) {
                this.draftStatus = 'Draft could not be saved. Browser storage is unavailable or full.';
            }
        },
        restoreDraft() {
            try {
                const value = localStorage.getItem('pixelit-creator-8x8-v1');
                if (!value) return;
                const draft = JSON.parse(value);
                if (!validFrames(draft.frames)) throw new Error('Invalid draft');
                this.frames = draft.frames;
                this.selectedFrame = Number.isInteger(draft.selectedFrame) ? Math.max(0, Math.min(draft.selectedFrame, draft.frames.length - 1)) : 0;
                this.previewDelay = Number.isInteger(draft.previewDelay) && draft.previewDelay >= 50 && draft.previewDelay <= 1000 ? draft.previewDelay : 200;
                this.onionSkin = draft.onionSkin === true;
                this.active8x8Background = this.frames[this.selectedFrame].map(rgb565ToHex);
                this.serializeFrames();
                this.draftStatus = 'Draft restored · Changes saved automatically';
            } catch (error) {
                this.draftStatus = 'Saved draft could not be restored.';
            }
        },
        paintPixel(index) {
            const wide = this.pixelMode === 1;
            if (wide ? this.bitmap32Error : this.animationError) return;
            const frame = wide ? this.bitmap32 : this.frames[this.selectedFrame];
            const width = wide ? 32 : 8;
            if (this.tool === 'picker') {
                this.colors = pixelColorFull(frame[index]);
                this.tool = 'pen';
                return;
            }
            const color = this.tool === 'eraser' ? 0 : rgb888ToRgb565(hexToRgb(this.colors));
            if (frame[index] === color) return;
            if (wide) this.recordHistory32();
            else this.recordHistory();
            this.stopPreview();
            if (this.tool === 'fill') {
                const original = frame[index];
                const pending = [index];
                while (pending.length) {
                    const current = pending.pop();
                    if (frame[current] !== original) continue;
                    this.$set(frame, current, color);
                    if (current % width > 0) pending.push(current - 1);
                    if (current % width < width - 1) pending.push(current + 1);
                    if (current >= width) pending.push(current - width);
                    if (current < frame.length - width) pending.push(current + width);
                }
            } else {
                this.$set(frame, index, color);
            }
            if (wide) {
                this.commitBitmap32();
            } else {
                this.active8x8Background = frame.map(rgb565ToHex);
                this.serializeFrames();
                this.sendLiveFrame(this.selectedFrame);
            }
        },
        clearFrame() {
            if (this.animationError || !this.frames[this.selectedFrame].some(pixel => pixel !== 0)) return;
            this.recordHistory();
            this.stopPreview();
            this.$set(this.frames, this.selectedFrame, Array(64).fill(0));
            this.active8x8Background = this.frames[this.selectedFrame].map(rgb565ToHex);
            this.serializeFrames();
            this.sendLiveFrame(this.selectedFrame);
        },
        sendLiveFrame(index, force = false) {
            if ((!this.livedraw && !force) || !this.sockedIsConnected || this.pixelMode !== 0 || this.animationError) return;
            this.$socket.sendObj({ setScreen: { bitmapAnimation: {
                data: [this.frames[index].slice()], animationDelay: this.previewDelay,
            } } });
        },
        restartPreview() {
            if (!this.playing) return;
            this.stopPreview();
            this.togglePreview();
        },
        selectFrameFromClick(index) {
            if (this.suppressFrameClick) {
                this.suppressFrameClick = false;
                return;
            }
            this.selectFrame(index);
        },
        startFrameDrag(event, index) {
            this.suppressFrameClick = false;
            if (event.button !== 0 || !event.isPrimary || this.animationError || this.frames.length < 2) return;
            const bounds = event.currentTarget.getBoundingClientRect();
            this.framePointer = {
                id: event.pointerId, index, x: event.clientX, y: event.clientY,
                offsetX: event.clientX - bounds.left, offsetY: event.clientY - bounds.top,
            };
            event.currentTarget.setPointerCapture(event.pointerId);
        },
        overFrame(event) {
            const pointer = this.framePointer;
            if (!pointer || pointer.id !== event.pointerId || this.animationError) return;
            this.frameDragPosition = { x: event.clientX - pointer.offsetX, y: event.clientY - pointer.offsetY };
            if (this.draggedFrame === null) {
                if (Math.hypot(event.clientX - pointer.x, event.clientY - pointer.y) < 5) return;
                this.stopPreview();
                this.draggedFrame = pointer.index;
                document.body.classList.add('creator-frame-grabbing');
            }
            const list = event.currentTarget.closest('.frame-list');
            const bounds = list.getBoundingClientRect();
            if (event.clientX < bounds.left + 24) list.scrollLeft -= 12;
            if (event.clientX > bounds.right - 24) list.scrollLeft += 12;
            this.dropPosition = null;
            if (event.clientY < bounds.top || event.clientY > bounds.bottom) return;
            const tiles = Array.from(list.querySelectorAll('[data-frame-index]'));
            const target = tiles.find(tile => {
                const rect = tile.getBoundingClientRect();
                return event.clientX < rect.left + rect.width / 2;
            });
            this.dropPosition = target ? Number(target.dataset.frameIndex) : this.frames.length;
        },
        dropFrame(event) {
            if (!this.framePointer || this.framePointer.id !== event.pointerId) return;
            const source = this.draggedFrame;
            const position = this.dropPosition;
            this.suppressFrameClick = source !== null;
            this.endFrameDrag();
            if (event.currentTarget.hasPointerCapture(event.pointerId)) {
                event.currentTarget.releasePointerCapture(event.pointerId);
            }
            if (source === null || position === null || this.animationError) return;
            const target = position - (position > source ? 1 : 0);
            this.selectFrame(source);
            this.moveFrame(target - source);
        },
        endFrameDrag() {
            this.framePointer = null;
            this.draggedFrame = null;
            this.dropPosition = null;
            if (typeof document !== 'undefined') document.body.classList.remove('creator-frame-grabbing');
        },
        stopPreview() {
            if (this.livePreview) this.sendLiveFrame(this.previewFrame, true);
            this.livePreview = false;
            clearInterval(this.previewTimer);
            this.previewTimer = null;
            this.playing = false;
        },
        togglePreview() {
            if (this.playing) {
                this.selectedFrame = this.previewFrame;
                this.active8x8Background = this.frames[this.selectedFrame].map(rgb565ToHex);
                this.stopPreview();
                this.saveDraft();
                return;
            }
            if (this.animationError || this.frames.length < 2 || this.pixelMode !== 0) return;
            if (this.livedraw && this.sockedIsConnected) {
                this.livePreview = true;
                this.$socket.sendObj({
                    setScreen: {
                        bitmapAnimation: {
                            data: this.frames.map(frame => frame.slice()),
                            animationDelay: this.previewDelay,
                        },
                    },
                });
            }
            this.previewFrame = 0;
            this.playing = true;
            this.previewTimer = setInterval(() => {
                this.previewFrame = (this.previewFrame + 1) % this.frames.length;
            }, this.previewDelay);
        },
        selectFrame(index) {
            this.stopPreview();
            this.selectedFrame = index;
            this.active8x8Background = this.frames[index].map(rgb565ToHex);
            this.saveDraft();
        },
        serializeFrames() {
            this.array8x8String = this.frames.map(frame => JSON.stringify(frame)).join(',\n');
            this.saveDraft();
        },
        moveFrame(direction) {
            const target = this.selectedFrame + direction;
            if (this.animationError || target < 0 || target >= this.frames.length || target === this.selectedFrame) return;
            this.recordHistory();
            this.stopPreview();
            const frame = this.frames.splice(this.selectedFrame, 1)[0];
            this.frames.splice(target, 0, frame);
            this.selectFrame(target);
            this.serializeFrames();
        },
        addFrame(duplicate) {
            if (this.animationError) return;
            this.recordHistory();
            this.stopPreview();
            this.frames.splice(this.selectedFrame + 1, 0, duplicate ? this.frames[this.selectedFrame].slice() : Array(64).fill(0));
            this.selectFrame(this.selectedFrame + 1);
            this.serializeFrames();
        },
        deleteFrame() {
            if (this.frames.length === 1 || this.animationError) return;
            this.recordHistory();
            this.stopPreview();
            this.frames.splice(this.selectedFrame, 1);
            this.selectFrame(Math.min(this.selectedFrame, this.frames.length - 1));
            this.serializeFrames();
        },
        changePixelModeTo8x8() {
            this.pixelMode = 0;
        },
        changePixelModeTo8x32() {
            this.stopPreview();
            this.pixelMode = 1;
        },
    },
};

function validBitmap(bitmap, size) {
    return Array.isArray(bitmap) && bitmap.length === size && bitmap.every(pixel => Number.isInteger(pixel) && pixel >= 0 && pixel <= 65535);
}

function validFrames(frames) {
    return Array.isArray(frames) && frames.length > 0 && frames.every(frame => validBitmap(frame, 64));
}

function pixelColorFull(value) {
    const r = Math.round(((value >> 11) & 31) * 255 / 31);
    const g = Math.round(((value >> 5) & 63) * 255 / 63);
    const b = Math.round((value & 31) * 255 / 31);
    return '#' + [r, g, b].map(channel => channel.toString(16).padStart(2, '0')).join('');
}

function rgb888ToRgb565(rgbArray) {
    return ((rgbArray.r & 0xf8) << 8) + ((rgbArray.g & 0xfc) << 3) + (rgbArray.b >> 3);
}

function rgb565ToHex(rgb565) {
    const red = ((rgb565 >> 11) & 0x1f) << 3;
    const green = ((rgb565 >> 5) & 0x3f) << 2;
    const blue = (rgb565 & 0x1f) << 3;

    return `#${[red, green, blue]
        .map((value) => value.toString(16).padStart(2, '0'))
        .join('')}`;
}

function hexToRgb(hex) {
    if (/^#[a-f\d]{8}$/i.test(hex)) hex = hex.slice(0, 7);
    const shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
    hex = hex.replace(shorthandRegex, function (m, r, g, b) {
        return r + r + g + g + b + b;
    });

    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result
        ? {
              r: parseInt(result[1], 16),
              g: parseInt(result[2], 16),
              b: parseInt(result[3], 16),
          }
        : null;
}
</script>
<style scoped>
.pixel-editor {
    display: grid;
    grid-template-columns: repeat(8, 1fr);
    width: 100%;
    max-width: 323px;
    aspect-ratio: 1;
    margin: auto;
    border: 1px solid #666;
}
.wide-editor-scroll { overflow-x: auto; }
.pixel-editor-wide {
    max-width: 803px;
    min-width: 640px;
    grid-template-columns: repeat(32, 1fr);
    aspect-ratio: 4;
}
.editor-pixel { position: relative; border: 1px solid #555; min-width: 0; }
.editor-pixel:focus-visible { outline: 2px solid white; outline-offset: -3px; z-index: 1; }
.onion-pixel { position: absolute; inset: 0; opacity: .3; pointer-events: none; }
.editor-tools, .editor-options { display: flex; justify-content: space-between; align-items: center; gap: 8px; flex-wrap: wrap; }
.editor-options { margin-top: 12px; }
.studio-heading,
.frame-toolbar,
.creator-footer {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
}
.studio-heading { margin-bottom: 16px; }
.frame-list {
    display: flex;
    gap: 10px;
    overflow-x: auto;
    padding: 4px 3px 12px;
}
.frame-tile {
    position: relative;
    flex: 0 0 76px;
    padding: 7px;
    border: 1px solid rgba(128, 128, 128, .3);
    border-radius: 10px;
    color: inherit;
    transition: border-color .15s, background-color .15s;
}
.frame-tile:hover { background: rgba(128, 128, 128, .1); }
.frame-reorderable { cursor: grab; touch-action: pan-x; user-select: none; }
.frame-reorderable:active,
.frame-list-dragging,
.frame-list-dragging * { cursor: grabbing !important; }
.frame-dragging { opacity: .4; }
.frame-drag-preview {
    position: fixed;
    z-index: 10000;
    width: 76px;
    pointer-events: none;
    text-align: center;
    background: var(--v-background-base, #212121);
    border-color: var(--v-primary-base, #1976d2);
    box-shadow: 0 6px 18px rgba(0, 0, 0, .35);
}
.frame-drop-before::before,
.frame-drop-after::after {
    content: '';
    position: absolute;
    top: 0;
    bottom: 0;
    width: 3px;
    border-radius: 3px;
    background: var(--v-primary-base, #1976d2);
}
.frame-drop-before::before { left: -7px; }
.frame-drop-after::after { right: -7px; }
.frame-tile:focus-visible { outline: 2px solid var(--v-primary-base, #1976d2); outline-offset: 2px; }
.frame-tile-active {
    border-color: var(--v-primary-base, #1976d2);
    box-shadow: 0 0 0 1px var(--v-primary-base, #1976d2);
    background: rgba(128, 128, 128, .08);
}
.frame-thumbnail,
.animation-preview {
    display: grid;
    grid-template-columns: repeat(8, 1fr);
    grid-template-rows: repeat(8, 1fr);
    overflow: hidden;
    background: #000;
}
.frame-thumbnail { width: 60px; height: 60px; border-radius: 3px; }
.frame-caption { display: block; margin-top: 6px; font-size: 11px; font-weight: 600; }
.frame-add { display: flex; flex-direction: column; align-items: center; justify-content: center; border-style: dashed; }
.frame-add:disabled { opacity: .4; cursor: default; }
.frame-toolbar { margin-bottom: 16px; }
.frame-toolbar { flex-wrap: wrap; }
.frame-actions { display: flex; align-items: center; }
.preview-panel {
    display: flex;
    align-items: center;
    gap: 20px;
    padding: 16px;
    border: 1px solid rgba(128, 128, 128, .2);
    border-radius: 12px;
    background: rgba(128, 128, 128, .05);
}
.animation-preview { width: 112px; height: 112px; flex-shrink: 0; border-radius: 4px; }
.preview-controls { min-width: 0; }
.creator-footer { padding-top: 16px; border-top: 1px solid rgba(128, 128, 128, .2); }
.bitmap-data { font-family: monospace; }
@media (max-width: 400px) {
    .preview-panel { gap: 12px; padding: 12px; flex-wrap: wrap; }
    .animation-preview { width: 88px; height: 88px; }
    .studio-heading { flex-wrap: wrap; }
}
#art,
.v-color-picker {
    padding-left: 0;
    padding-right: 0;
    margin-left: auto;
    margin-right: auto;
    display: block;
}
</style>

<style>
body.creator-frame-grabbing,
body.creator-frame-grabbing * { cursor: grabbing !important; user-select: none !important; }
</style>

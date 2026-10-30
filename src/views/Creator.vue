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
        <v-row>
            <v-col cols="12" :lg="pixelMode === 0 ? 4 : 8" :offset-lg="pixelMode === 0 ? 2 : 0">
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
                    <div class="wide-editor-scroll">
                    <div class="pixel-editor" :class="{ 'pixel-editor-wide': pixelMode === 1 }" role="group" :aria-label="canvasWidth + ' by 8 pixel editor'"
                        @pointerdown="startPaintStroke" @pointermove="continuePaintStroke" @pointerup="endPaintStroke"
                        @pointercancel="endPaintStroke" @lostpointercapture="endPaintStroke">
                        <button v-for="(pixel, index) in frames[selectedFrame]" :key="index" type="button" class="editor-pixel"
                            :style="{ backgroundColor: pixelColor(pixel) }" :aria-label="'Pixel ' + (index + 1)" :data-pixel-index="index" @click="keyboardPaint($event, index)">
                            <span v-if="onionSkin && selectedFrame > 0 && pixel === 0" class="onion-pixel"
                                :style="{ backgroundColor: pixelColor(frames[selectedFrame - 1][index]) }"></span>
                        </button>
                    </div>
                    </div>
                    <div class="editor-options">
                        <v-switch v-model="onionSkin" label="Previous frame overlay" :disabled="selectedFrame === 0" dense hide-details class="mt-0"></v-switch>
                        <v-btn small text :disabled="!!animationError || !frames[selectedFrame].some(pixel => pixel !== 0)" @click="clearFrame">Clear frame</v-btn>
                    </div>
                    <div class="text-caption text--secondary mt-2" role="status">{{ draftStatus }}</div>
                    <v-alert v-if="frames.length >= frameLimit" dense text :type="frameLimitExceeded ? 'error' : 'info'" class="mt-3">
                        {{ frameLimitExceeded ? 'This draft exceeds the frame limit. Delete frames to enable playback and saving.' : 'Frame limit reached.' }}
                        Maximum: {{ frameLimit }} frames for {{ canvasWidth }}×8.
                    </v-alert>
                    <section class="animation-studio mt-5">
                        <div class="studio-heading">
                            <div>
                                <div class="text-overline primary--text">ANIMATION</div>
                                <div class="text-subtitle-1 font-weight-bold">Bring your pixels to life</div>
                            </div>
                            <v-chip small outlined>{{ frames.length }} / {{ frameLimit }} frames</v-chip>
                        </div>
                        <div class="frame-list" :class="{ 'frame-list-dragging': draggedFrame !== null }" role="group" aria-label="Animation frames">
                            <button v-for="(frame, index) in frames" :key="index" type="button"
                                class="frame-tile" :class="{ 'frame-tile-wide': pixelMode === 1, 'frame-tile-active': selectedFrame === index, 'frame-dragging': draggedFrame === index,
                                    'frame-reorderable': !animationError && frames.length > 1, 'frame-drop-before': dropPosition === index, 'frame-drop-after': dropPosition === frames.length && index === frames.length - 1 }"
                                :draggable="false" :data-frame-index="index"
                                @dragstart.prevent @pointerdown="startFrameDrag($event, index)"
                                @pointermove="overFrame" @pointerup="dropFrame"
                                @pointercancel="endFrameDrag" @lostpointercapture="endFrameDrag"
                                :aria-label="'Edit frame ' + (index + 1)" :aria-pressed="selectedFrame === index" @click="selectFrameFromClick(index)">
                                <span :class="['frame-thumbnail', { 'frame-thumbnail-wide': pixelMode === 1 }]">
                                    <span v-for="(pixel, pixelIndex) in frame" :key="pixelIndex" :style="{ backgroundColor: pixelColor(pixel) }"></span>
                                </span>
                                <span class="frame-caption">{{ String(index + 1).padStart(2, '0') }}</span>
                            </button>
                            <button type="button" class="frame-tile frame-add" aria-label="Add empty frame" :disabled="!!animationError || frames.length >= frameLimit" @click="addFrame(false)">
                                <v-icon>mdi-plus</v-icon>
                                <span class="frame-caption">New</span>
                            </button>
                        </div>
                        <div v-if="draggedFrame !== null" class="frame-tile frame-drag-preview" :class="{ 'frame-tile-wide': pixelMode === 1 }" aria-hidden="true"
                            :style="{ left: frameDragPosition.x + 'px', top: frameDragPosition.y + 'px' }">
                            <span :class="['frame-thumbnail', { 'frame-thumbnail-wide': pixelMode === 1 }]">
                                <span v-for="(pixel, pixelIndex) in frames[draggedFrame]" :key="pixelIndex" :style="{ backgroundColor: pixelColor(pixel) }"></span>
                            </span>
                            <span class="frame-caption">{{ String(draggedFrame + 1).padStart(2, '0') }}</span>
                        </div>
                        <div class="frame-toolbar">
                            <span class="text-caption text--secondary">Editing frame {{ selectedFrame + 1 }}</span>
                            <div class="frame-actions">
                                <v-btn icon small :disabled="selectedFrame === 0 || !!animationError" aria-label="Move selected frame left" title="Move frame left" @click="moveFrame(-1)"><v-icon small>mdi-arrow-left</v-icon></v-btn>
                                <v-btn icon small :disabled="selectedFrame === frames.length - 1 || !!animationError" aria-label="Move selected frame right" title="Move frame right" @click="moveFrame(1)"><v-icon small>mdi-arrow-right</v-icon></v-btn>
                                <v-btn small text :disabled="!!animationError || frames.length >= frameLimit" @click="addFrame(true)"><v-icon small left>mdi-content-copy</v-icon>Duplicate</v-btn>
                                <v-btn icon small :disabled="frames.length === 1 || !!animationError" aria-label="Delete selected frame" title="Delete selected frame" @click="deleteFrame"><v-icon small>mdi-trash-can-outline</v-icon></v-btn>
                            </div>
                        </div>
                        <div class="preview-panel">
                            <div :class="['animation-preview', { 'animation-preview-wide': pixelMode === 1 }]" aria-label="Animation preview">
                                <div v-for="(color, index) in previewBackground" :key="index" :style="{ backgroundColor: color }"></div>
                            </div>
                            <div class="preview-controls">
                                <div class="text-subtitle-2">{{ playing ? 'Playing animation' : 'Animation preview' }}</div>
                                <div class="text-caption text--secondary mb-3">{{ frames.length > 1 ? (1000 / previewDelay).toFixed(1) + ' fps · Loop' : 'Add a frame to start animating' }}</div>
                                <v-btn small rounded depressed color="primary" :disabled="frames.length < 2 || !!animationError || frameLimitExceeded" @click="togglePreview">
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
                                <v-textarea outlined v-model="bitmapText" label="RGB565 frames" :error-messages="animationError" rows="4" class="bitmap-data"></v-textarea>
                            </v-expansion-panel-content>
                        </v-expansion-panel>
                    </v-expansion-panels>
                    <v-alert v-if="animationError" dense text type="error" class="mt-2">{{ animationError }}</v-alert>
                    <div class="creator-footer">
                        <v-switch class="mt-0 pt-0" v-model="livedraw" label="Live draw" :disabled="!sockedIsConnected || !!animationError" hide-details dense></v-switch>
                        <ButtonSave color="green" text="Save" icon="mdi-content-save" :data="bitmapText" :pixelMode="pixelMode" :condition="!animationError && !frameLimitExceeded"></ButtonSave>
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="ui-panel" elevation="2">
                    <v-color-picker v-model="colors" mode="hexa" dot-size="20" show-swatches swatches-max-height="280"></v-color-picker>
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
            canvases: [createCanvas(64), createCanvas(256)],
            array8x8String: `[${Array(64).fill(0).join(',')}]`,
            array8x32String: `[${Array(256).fill(0).join(',')}]`,
            livedraw: false,
            previewFrame: 0,
            playing: false,
            previewTimer: null,
            draggedFrame: null,
            framePointer: null,
            frameDragPosition: { x: 0, y: 0 },
            suppressFrameClick: false,
            dropPosition: null,
            tool: 'pen',
            paintStroke: null,
            livePreview: false,
        };
    },
    components: {
        ButtonSave,
    },
    mounted() {
        this.restoreDraft();
        this.restoreDraft(1);
        window.addEventListener('blur', this.endPaintStroke);
    },
    beforeDestroy() {
        this.endPaintStroke();
        if (typeof window !== 'undefined') window.removeEventListener('blur', this.endPaintStroke);
        this.endFrameDrag();
        this.stopPreview();
    },
    computed: {
        frames: canvasProperty('frames'),
        selectedFrame: canvasProperty('selectedFrame'),
        animationError: canvasProperty('animationError'),
        onionSkin: canvasProperty('onionSkin'),
        previewDelay: canvasProperty('previewDelay'),
        undoStack: canvasProperty('undoStack'),
        redoStack: canvasProperty('redoStack'),
        draftStatus: canvasProperty('draftStatus'),
        frameLimit() { return this.pixelMode === 0 ? 28 : 7; },
        frameLimitExceeded() { return this.frames.length > this.frameLimit; },
        canvasWidth() { return this.pixelMode === 0 ? 8 : 32; },
        bitmapText: {
            get() { return this.pixelMode === 0 ? this.array8x8String : this.array8x32String; },
            set(value) {
                if (this.pixelMode === 0) this.array8x8String = value;
                else this.array8x32String = value;
            },
        },
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
        array8x8String(value) { this.importFrames(value, 0); },
        array8x32String(value) { this.importFrames(value, 1); },
    },
    methods: {
        pixelColor: rgb565ToHex,
        importFrames(value, mode) {
            const canvas = this.canvases[mode];
            try {
                const parsed = JSON.parse(`[${value.trim().replace(/,$/, '')}]`);
                const unchanged = JSON.stringify(parsed) === JSON.stringify(canvas.frames);
                if (!validFrames(parsed, mode === 0 ? 64 : 256, !unchanged)) throw new Error('Invalid frames');
                if (!unchanged) {
                    if (this.pixelMode === mode) this.stopPreview();
                    canvas.undoStack.push({ frames: canvas.frames.map(frame => frame.slice()), selectedFrame: canvas.selectedFrame });
                    if (canvas.undoStack.length > 100) canvas.undoStack.shift();
                    canvas.redoStack = [];
                    canvas.frames = parsed;
                    canvas.selectedFrame = Math.min(canvas.selectedFrame, parsed.length - 1);
                }
                canvas.animationError = '';
                this.saveDraft(mode);
            } catch (error) {
                if (this.pixelMode === mode) this.stopPreview();
                canvas.animationError = `Enter at most ${mode === 0 ? 28 : 7} frames of ${mode === 0 ? 64 : 256} RGB565 values (0–65535), separated by commas.`;
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
        saveDraft(mode = this.pixelMode) {
            const canvas = this.canvases[mode];
            try {
                localStorage.setItem(mode === 0 ? 'pixelit-creator-8x8-v1' : 'pixelit-creator-8x32-v2', JSON.stringify({
                    frames: canvas.frames, selectedFrame: canvas.selectedFrame,
                    previewDelay: canvas.previewDelay, onionSkin: canvas.onionSkin,
                }));
                canvas.draftStatus = 'Draft saved in this browser';
            } catch (error) {
                canvas.draftStatus = 'Draft could not be saved. Browser storage is unavailable or full.';
            }
        },
        restoreDraft(mode = this.pixelMode) {
            const canvas = this.canvases[mode];
            try {
                let value = localStorage.getItem(mode === 0 ? 'pixelit-creator-8x8-v1' : 'pixelit-creator-8x32-v2');
                if (!value && mode === 1) {
                    const legacy = localStorage.getItem('pixelit-creator-8x32-v1');
                    if (legacy) value = JSON.stringify({ frames: [JSON.parse(legacy)] });
                }
                if (!value) return;
                const draft = JSON.parse(value);
                if (!validFrames(draft.frames, mode === 0 ? 64 : 256, false)) throw new Error('Invalid draft');
                canvas.frames = draft.frames;
                canvas.selectedFrame = Number.isInteger(draft.selectedFrame) ? Math.max(0, Math.min(draft.selectedFrame, draft.frames.length - 1)) : 0;
                canvas.previewDelay = Number.isInteger(draft.previewDelay) && draft.previewDelay >= 50 && draft.previewDelay <= 1000 ? draft.previewDelay : 200;
                canvas.onionSkin = draft.onionSkin === true;
                const text = canvas.frames.map(frame => JSON.stringify(frame)).join(',\n');
                if (mode === 0) this.array8x8String = text;
                else this.array8x32String = text;
                canvas.draftStatus = 'Draft restored · Changes saved automatically';
            } catch (error) {
                canvas.draftStatus = 'Saved draft could not be restored.';
            }
        },
        keyboardPaint(event, index) {
            if (event.detail === 0) this.paintPixel(index);
        },
        startPaintStroke(event) {
            if (event.button !== 0 || !event.isPrimary || this.animationError) return;
            const pixel = event.target.closest('[data-pixel-index]');
            if (!pixel) return;
            event.preventDefault();
            const index = Number(pixel.dataset.pixelIndex);
            this.endPaintStroke();
            if (this.tool !== 'pen' && this.tool !== 'eraser') {
                this.paintPixel(index);
                return;
            }
            this.paintStroke = {
                pointerId: event.pointerId, mode: this.pixelMode, frame: this.selectedFrame,
                tool: this.tool, last: index, recorded: false,
            };
            event.currentTarget.setPointerCapture(event.pointerId);
            this.paintPixel(index);
        },
        continuePaintStroke(event) {
            const stroke = this.paintStroke;
            if (!stroke || stroke.pointerId !== event.pointerId) return;
            if (!(event.buttons & 1) || stroke.mode !== this.pixelMode || stroke.frame !== this.selectedFrame || stroke.tool !== this.tool) {
                this.endPaintStroke();
                return;
            }
            event.preventDefault();
            const bounds = event.currentTarget.getBoundingClientRect();
            const x = Math.floor((event.clientX - bounds.left) / bounds.width * this.canvasWidth);
            const y = Math.floor((event.clientY - bounds.top) / bounds.height * 8);
            if (x < 0 || x >= this.canvasWidth || y < 0 || y >= 8) {
                stroke.last = null;
                return;
            }
            const index = y * this.canvasWidth + x;
            if (stroke.last === index) return;
            // Interpolate skipped cells so fast strokes remain continuous.
            let fromX = stroke.last === null ? x : stroke.last % this.canvasWidth;
            let fromY = stroke.last === null ? y : Math.floor(stroke.last / this.canvasWidth);
            const dx = Math.abs(x - fromX), dy = -Math.abs(y - fromY);
            const stepX = fromX < x ? 1 : -1, stepY = fromY < y ? 1 : -1;
            let error = dx + dy;
            for (let remaining = this.canvasWidth + 8; remaining > 0; remaining--) {
                this.paintPixel(fromY * this.canvasWidth + fromX, false);
                if (fromX === x && fromY === y) break;
                const doubled = 2 * error;
                if (doubled >= dy) { error += dy; fromX += stepX; }
                if (doubled <= dx) { error += dx; fromY += stepY; }
            }
            stroke.last = index;
            this.serializeFrames();
            this.sendLiveFrame(this.selectedFrame);
        },
        endPaintStroke(event) {
            if (event && event.pointerId !== undefined && this.paintStroke && event.pointerId !== this.paintStroke.pointerId) return;
            this.paintStroke = null;
        },
        paintPixel(index, commit = true) {
            if (this.animationError) return;
            const frame = this.frames[this.selectedFrame];
            const width = this.canvasWidth;
            if (this.tool === 'picker') {
                this.colors = pixelColorFull(frame[index]);
                this.tool = 'pen';
                return;
            }
            const color = this.tool === 'eraser' ? 0 : rgb888ToRgb565(hexToRgb(this.colors));
            if (frame[index] === color) return;
            if (!this.paintStroke || !this.paintStroke.recorded) {
                this.recordHistory();
                if (this.paintStroke) this.paintStroke.recorded = true;
            }
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
            if (commit) {
                this.serializeFrames();
                this.sendLiveFrame(this.selectedFrame);
            }
        },
        clearFrame() {
            if (this.animationError || !this.frames[this.selectedFrame].some(pixel => pixel !== 0)) return;
            this.recordHistory();
            this.stopPreview();
            this.$set(this.frames, this.selectedFrame, Array(this.canvasWidth * 8).fill(0));
            this.serializeFrames();
            this.sendLiveFrame(this.selectedFrame);
        },
        sendLiveFrame(index, force = false) {
            if ((!this.livedraw && !force) || !this.sockedIsConnected || this.animationError || this.frameLimitExceeded) return;
            const data = this.frames[index].slice();
            if (this.pixelMode === 1) {
                this.$socket.sendObj({ setScreen: { bitmap: {
                    data, position: { x: 0, y: 0 }, size: { width: 32, height: 8 },
                } } });
            } else {
                this.$socket.sendObj({ setScreen: { bitmapAnimation: {
                    data: [data], animationDelay: this.previewDelay,
                } } });
            }
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
                this.stopPreview();
                this.saveDraft();
                return;
            }
            if (this.animationError || this.frames.length < 2 || this.frameLimitExceeded) return;
            if (this.livedraw && this.sockedIsConnected) {
                this.livePreview = true;
                this.$socket.sendObj({
                    setScreen: {
                        bitmapAnimation: {
                            data: this.frames.map(frame => frame.slice()),
                            animationDelay: this.previewDelay,
                            size: { width: this.canvasWidth, height: 8 },
                            position: { x: 0, y: 0 },
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
            this.endPaintStroke();
            this.stopPreview();
            this.selectedFrame = index;
            this.saveDraft();
        },
        serializeFrames() {
            this.bitmapText = this.frames.map(frame => JSON.stringify(frame)).join(',\n');
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
            if (this.animationError || this.frames.length >= this.frameLimit) return;
            this.recordHistory();
            this.stopPreview();
            this.frames.splice(this.selectedFrame + 1, 0, duplicate ? this.frames[this.selectedFrame].slice() : Array(this.canvasWidth * 8).fill(0));
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
            this.endPaintStroke();
            this.endFrameDrag();
            this.stopPreview();
            this.pixelMode = 0;
        },
        changePixelModeTo8x32() {
            this.endPaintStroke();
            this.endFrameDrag();
            this.stopPreview();
            this.pixelMode = 1;
        },
    },
};

function validBitmap(bitmap, size) {
    return Array.isArray(bitmap) && bitmap.length === size && bitmap.every(pixel => Number.isInteger(pixel) && pixel >= 0 && pixel <= 65535);
}

function createCanvas(size) {
    return {
        frames: [Array(size).fill(0)], selectedFrame: 0, animationError: '',
        onionSkin: false, previewDelay: 200, undoStack: [], redoStack: [],
        draftStatus: 'Draft saved automatically in this browser',
    };
}

function canvasProperty(key) {
    return {
        get() { return this.canvases[this.pixelMode][key]; },
        set(value) { this.canvases[this.pixelMode][key] = value; },
    };
}

function validFrames(frames, size = 64, enforceLimit = true) {
    return Array.isArray(frames) && frames.length > 0 && (!enforceLimit || frames.length <= (size === 64 ? 28 : 7)) && frames.every(frame => validBitmap(frame, size));
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
    touch-action: none;
    user-select: none;
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
.frame-tile-wide { flex-basis: 140px; }
.frame-thumbnail-wide { width: 124px; height: 31px; grid-template-columns: repeat(32, 1fr); }
.frame-drag-preview.frame-tile-wide { width: 140px; }
.animation-preview.animation-preview-wide { width: 224px; height: 56px; grid-template-columns: repeat(32, 1fr); }
.frame-caption { display: block; margin-top: 6px; font-size: 11px; font-weight: 600; }
.frame-add { display: flex; flex-direction: column; align-items: center; justify-content: center; border-style: dashed; }
.frame-add:disabled { opacity: .4; cursor: default; }
.frame-toolbar { margin-bottom: 16px; }
.frame-toolbar { flex-wrap: wrap; }
.frame-actions { display: flex; align-items: center; }
.preview-panel {
    display: flex;
    flex-wrap: wrap;
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

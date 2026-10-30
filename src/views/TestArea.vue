<template>
    <v-container class="home app-page">
        <v-row>
            <v-col cols="12" lg="4">
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Liveview</h2>
                    </v-card-title>

                    <Liveview class="text-center" :data="liveview" :options="liveviewCanvasSettings" />
                </v-card>
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>JSON String</h2>
                    </v-card-title>

                    <prism-editor class="editor" v-model="testarea.json" :highlight="highlighter" line-numbers></prism-editor>
                    <br />
                    <div class="text-center">
                        <ButtonCondition color="info" :condition="sockedIsConnected" text="Send JSON" icon="mdi-cloud-upload" :onclick="sendJson" />
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Text / Brightness</h2>
                    </v-card-title>

                    <span class="text-h2 font-weight-light" v-text="testarea.brightness"></span>
                    <span class="subheading font-weight-light mr-1">Brightness</span>
                    <v-slider v-model="testarea.brightness" max="255" min="0" @end="sendBrightness"></v-slider>
                    <v-text-field label="Text to send" v-model="testarea.text"></v-text-field>
                    <div class="text-center">
                        <ButtonCondition color="info" :condition="sockedIsConnected" text="Send Text" icon="mdi-cloud-upload" :onclick="sendText" />
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="ui-panel" elevation="2">
                    <v-card-title>
                        <h2>Bitmap Array</h2>
                    </v-card-title>

                    <prism-editor class="editor" v-model="testarea.image" :highlight="highlighter"></prism-editor>
                    <p class="text-caption text--secondary mt-2">
                        Size detected automatically: 64 values = 8×8 (up to 28 frames),
                        256 values = 32×8 (up to 7 frames). Separate animation frames with commas.
                    </p>
                    <br />
                    <div class="text-center">
                        <ButtonCondition color="info" :condition="sockedIsConnected" text="Send Bitmap" icon="mdi-cloud-upload" :onclick="sendBitmap" />
                    </div>
                </v-card>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
import actionFeedback from '../mixins/actionFeedback';
import ButtonCondition from '../components/ButtonCondition';
import { PrismEditor } from 'vue-prism-editor';
import 'vue-prism-editor/dist/prismeditor.min.css';
import { highlight, languages } from 'prismjs/components/prism-core';
import 'prismjs/components/prism-clike';
import 'prismjs/components/prism-json';
import 'prismjs/themes/prism-tomorrow.css';
import Liveview from '../components/Liveview';

export default {
    mixins: [actionFeedback],
    feedbackActions: { sendJson: 'JSON command sent.', sendText: 'Text sent to PixelIt.', sendBitmap: 'Bitmap sent to PixelIt.', sendBrightness: null },
    name: 'TestArea',
    components: { ButtonCondition, PrismEditor, Liveview },
    computed: {
        testarea() {
            return this.$store.state.testarea;
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        liveview() {
            return this.$store.state.liveviewData;
        },
        liveviewCanvasSettings() {
            return this.$store.state.matrixSize;
        },
    },
    methods: {
        highlighter(code) {
            return highlight(code, languages.json);
        },
        sendJson() {
            this.$socket.sendObj({ forcedDuration: 5000, setScreen: JSON.parse(this.testarea.json) });
        },
        sendText() {
            this.$socket.sendObj({
                forcedDuration: 5000,
                setScreen: {
                    text: {
                        textString: this.testarea.text,
                        scrollText: 'auto',
                        hexColor: '#FFFFFF',
                        position: {
                            x: 0,
                            y: 1,
                        },
                    },
                },
            });
        },
        sendBitmap() {
            const input = this.testarea.image.trim().replace(/,$/, '');
            let frames;
            try {
                const parsed = JSON.parse(input);
                frames = Array.isArray(parsed) && typeof parsed[0] === 'number' ? [parsed] : parsed;
            } catch (error) {
                frames = JSON.parse(`[${input}]`);
            }
            if (!Array.isArray(frames) || !frames.length || !Array.isArray(frames[0]) ||
                ![64, 256].includes(frames[0].length)) {
                throw new Error('Each bitmap frame must contain 64 (8×8) or 256 (32×8) RGB565 values.');
            }
            const size = frames[0].length;
            const width = size === 64 ? 8 : 32;
            const limit = size === 64 ? 28 : 7;
            if (frames.length > limit) throw new Error(`Maximum ${limit} frames for ${width}×8.`);
            if (!frames.every(frame => Array.isArray(frame) && frame.length === size &&
                frame.every(pixel => Number.isInteger(pixel) && pixel >= 0 && pixel <= 65535))) {
                throw new Error('All frames must have the same size and contain RGB565 integers from 0 to 65535.');
            }
            const dimensions = { size: { width, height: 8 }, position: { x: 0, y: 0 } };
            this.$socket.sendObj({
                forcedDuration: 5000,
                setScreen: width === 32 && frames.length === 1 ? {
                    bitmap: { ...dimensions, data: frames[0] },
                } : {
                    bitmapAnimation: {
                        ...dimensions,
                        data: frames,
                        animationDelay: 200,
                    },
                },
            });
        },
        sendBrightness(value) {
            this.$socket.sendObj({
                setScreen: {
                    brightness: value,
                },
            });
        },
    },
};
</script>

<style>
.editor {
    background: #2d2d2d;
    color: #ccc;
    font-family: Fira code, Fira Mono, Consolas, Menlo, Courier, monospace;
    font-size: 14px;
    line-height: 1.5;
    padding: 5px;
    min-height: 200px;
    border-radius: 5px;
}
</style>

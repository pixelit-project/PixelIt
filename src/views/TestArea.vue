<template>
    <v-container class="home">
        <v-row>
            <v-col cols="12" lg="4">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Liveview</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <Liveview class="text-center" :data="liveview" :options="liveviewCanvasSettings" />
                </v-card>
                <br />
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>JSON String</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <prism-editor class="editor" v-model="testarea.json" :highlight="highlighter" line-numbers></prism-editor>
                    <br />
                    <div class="text-center">
                        <ButtonCondition color="info" :condition="sockedIsConnected" text="Send JSON" icon="mdi-cloud-upload" :onclick="sendJson" />
                    </div>
                </v-card>
            </v-col>
            <v-col cols="12" lg="4">
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Text / Brightness</h2>
                    </v-card-title>
                    <hr />
                    <br />
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
                <v-card class="pa-2" elevation="4">
                    <v-card-title>
                        <h2>Bitmap Array</h2>
                    </v-card-title>
                    <hr />
                    <br />
                    <prism-editor class="editor" v-model="testarea.image" :highlight="highlighter"></prism-editor>
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
import ButtonCondition from '../components/ButtonCondition';
import { PrismEditor } from 'vue-prism-editor';
import 'vue-prism-editor/dist/prismeditor.min.css';
import { highlight, languages } from 'prismjs/components/prism-core';
import 'prismjs/components/prism-clike';
import 'prismjs/components/prism-json';
import 'prismjs/themes/prism-tomorrow.css';
import Liveview from '../components/Liveview';

export default {
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
            this.$socket.sendObj({
                forcedDuration: 5000,
                setScreen: {
                    bitmapAnimation: {
                        data: JSON.parse(`[${this.testarea.image}]`),
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

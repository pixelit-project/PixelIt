<template>
    <div class="Liveview">
        <canvas ref="canvas" canvas :width="canvasSize.width" :height="canvasSize.height"></canvas>
    </div>
</template>

<script>
export default {
    name: 'Liveview',
    props: {
        data: {
            type: Array,
            required: true,
        },
        options: {
            type: Object,
            default: () => {},
        },
    },
    mounted() {
        this.canvas = this.$refs.canvas;
        this.ctx = this.canvas.getContext('2d');
        this.drawCanvas(); // Call the drawing method
    },
    watch: {
        data: 'drawCanvas', // Watch the data prop and call drawCanvas when it changes
    },
    computed: {
        canvasSettings() {
            // Merge the default options with the options passed in as a prop
            return { ...this.defaultOptions, ...this.options };
        },
        canvasSize() {
            // console.log('canvasSize', this.canvasSettings.cols, this.canvasSettings.rows, this.canvasSettings.rectWidth, this.canvasSettings.rectHeight, this.canvasSettings.padding);
            return {
                width: this.canvasSettings.cols * (this.canvasSettings.rectWidth + this.canvasSettings.padding) - 1,
                height: this.canvasSettings.rows * (this.canvasSettings.rectHeight + this.canvasSettings.padding) - 1,
            };
        },
    },
    data() {
        return {
            defaultOptions: {
                rectWidth: 12,
                rectHeight: 12,
                padding: 1,
                cols: 8,
                rows: 8,
            },
            canvas: null,
            ctx: null,
        };
    },
    methods: {
        drawCanvas() {
            // console.log('Drawing canvas from state', this.data);
            const rectWidth = this.canvasSettings.rectWidth;
            const rectHeight = this.canvasSettings.rectHeight;
            const padding = this.canvasSettings.padding;
            const rows = this.canvasSettings.rows;
            const cols = this.canvasSettings.cols;

            this.ctx.fillStyle = '#222';
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

            for (let row = 0; row < rows; row++) {
                for (let col = 0; col < cols; col++) {
                    const index = row * cols + col;

                    const color = this.data.length === 0 ? '#000' : '#' + this.data[index];
                    const x = col * (rectWidth + padding);
                    const y = row * (rectHeight + padding);
                    this.ctx.fillStyle = color;
                    this.ctx.fillRect(x, y, rectWidth, rectHeight);
                }
            }
        },
    },
};
</script>

<style>
[canvas] {
    border: 2px solid black;
    padding: 1px;
    border-radius: 5px;
    max-width: 100%;
}
</style>

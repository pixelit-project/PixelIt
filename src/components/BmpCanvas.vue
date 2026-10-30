<template>
    <canvas :height="height ? height : 150" :width="width ? width : bmp.sizeX == 8 ? 150 : 230"></canvas>
</template>

<script>
export default {
    props: {
        bmp: {
            type: Object,
            required: true,
        },
        height: {
            type: String,
            required: false,
        },
        width: {
            type: String,
            required: false,
        },
    },
    mounted() {
        // Keep parsed frames and timers outside Vue's reactive state.
        this.frames = null;
        this.frameIndex = 0;
        this.animationTimer = null;
        this.isVisible = false;
        document.addEventListener('visibilitychange', this.updateAnimation);

        if (typeof IntersectionObserver === 'undefined') {
            this.isVisible = true;
            this.prepareFrames();
            this.updateAnimation();
            return;
        }

        this.preloadObserver = new IntersectionObserver((entries) => {
            if (entries.some((entry) => entry.isIntersecting)) {
                this.prepareFrames();
                this.preloadObserver.disconnect();
            }
        }, { rootMargin: '800px' });
        this.preloadObserver.observe(this.$el);

        this.visibilityObserver = new IntersectionObserver((entries) => {
            this.isVisible = entries[0].isIntersecting;
            if (this.isVisible) {
                this.prepareFrames();
            }
            this.updateAnimation();
        });
        this.visibilityObserver.observe(this.$el);
    },
    beforeDestroy() {
        this.stopAnimation();
        if (this.preloadObserver) this.preloadObserver.disconnect();
        if (this.visibilityObserver) this.visibilityObserver.disconnect();
        document.removeEventListener('visibilitychange', this.updateAnimation);
    },
    watch: {
        bmp: {
            deep: true,
            handler() {
                this.stopAnimation();
                this.frames = null;
                this.frameIndex = 0;
                if (this.isVisible) {
                    this.prepareFrames();
                    this.updateAnimation();
                }
            },
        },
        width() {
            this.$nextTick(this.redraw);
        },
        height() {
            this.$nextTick(this.redraw);
        },
    },
    methods: {
        prepareFrames() {
            if (this.frames) return;
            let bmpArrayString = this.bmp.rgB565Array;
            if (bmpArrayString.endsWith(',')) {
                bmpArrayString = bmpArrayString.slice(0, -1);
            }
            this.frames = JSON.parse(`[${bmpArrayString}]`);
            this.redraw();
        },
        stopAnimation() {
            if (this.animationTimer !== null) {
                clearInterval(this.animationTimer);
                this.animationTimer = null;
            }
        },
        updateAnimation() {
            this.stopAnimation();
            if (!this.isVisible || document.hidden || !this.frames || this.frames.length <= 1) return;
            this.animationTimer = setInterval(() => {
                this.frameIndex = (this.frameIndex + 1) % this.frames.length;
                this.redraw();
            }, 250);
        },
        redraw() {
            if (!this.frames || !this.frames.length) return;
            const bmp = this.frames[this.frameIndex];
            const canvas = this.$el;
            const ctx = canvas.getContext('2d');
            const sizeX = this.bmp.sizeX;
            const sizeY = this.bmp.sizeY;
            const size = canvas.width / sizeX;

            ctx.clearRect(0, 0, canvas.width, canvas.height);
            const height = sizeX == 8 ? sizeY : sizeY + 6;
            let i = 0;
            for (let y = sizeX == 8 ? 0 : 6; y < height; y++) {
                for (let x = 0; x < sizeX; x++) {
                    const rgb = RGB565IntToRGB(bmp[i++]);
                    ctx.fillStyle = RGBToHEX(rgb[0], rgb[1], rgb[2]);
                    ctx.fillRect(x * size, y * size, size, size);
                }
            }
        },
    },
};

function RGB565IntToRGB(color) {
    const r = (((color >> 11) & 0x1f) * 527 + 23) >> 6;
    const g = (((color >> 5) & 0x3f) * 259 + 33) >> 6;
    const b = ((color & 0x1f) * 527 + 23) >> 6;
    return [r, g, b];
}

function RGBToHEX(red, green, blue) {
    const rgb = blue | (green << 8) | (red << 16);
    return '#' + (0x1000000 + rgb).toString(16).slice(1);
}
</script>

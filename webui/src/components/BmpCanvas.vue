<template>
    <canvas :height="height ? height : 150" :width="width ? width : bmp.sizeX == 8 ? 150 : 230" v-insertbmp="bmp"></canvas>
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
    directives: {
        insertbmp: function(canvasElement, binding) {
            let bmpArrayString = binding.value.rgB565Array;
            const sizeX = binding.value.sizeX;
            const sizeY = binding.value.sizeY;

            if (bmpArrayString.endsWith(",")) {
                bmpArrayString = bmpArrayString.slice(0, -1);
            }

            const bmpArray = JSON.parse(`[${bmpArrayString}]`);
            let bmpsLoopCounter = 0;
            let bmpsLoopInterval;
            if (bmpArray.length > 1) {
                if (bmpsLoopInterval) {
                    clearInterval(bmpsLoopInterval);
                }
                bmpsLoopInterval = setInterval(() => {
                    if (bmpArray.length == bmpsLoopCounter) {
                        bmpsLoopCounter = 0;
                    }
                    drawCanvas(bmpArray[bmpsLoopCounter]);
                    bmpsLoopCounter++;
                }, 200);
                for (const bmp of bmpArray) {
                    drawCanvas(bmp);
                }
            } else {
                drawCanvas(bmpArray[0]);
            }

            function drawCanvas(bmp) {
                const ctx = canvasElement.getContext("2d");
                const canvasWidth = canvasElement.width;
                const canvasHeight = canvasElement.height;
                const size = canvasWidth / sizeX;

                ctx.clearRect(0, 0, canvasWidth, canvasHeight); // clear canvas

                const width = sizeX;
                const height = sizeX == 8 ? sizeY : sizeY + 6;
                let i = 0;
                for (let y = sizeX == 8 ? 0 : 6; y < height; y++) {
                    for (let x = 0; x < width; x++) {
                        const rgb = RGB565IntToRGB(bmp[i++]);
                        let color = RGBToHEX(rgb[0], rgb[1], rgb[2]);
                        ctx.fillStyle = color;
                        ctx.fillRect(x * size, y * size, size, size);
                    }
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
    return "#" + (0x1000000 + rgb).toString(16).slice(1);
}
</script>

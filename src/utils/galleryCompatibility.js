// Cache only small metadata, never parsed pixel arrays for the whole gallery.
const metadataCache = new WeakMap();
const positive = (value, fallback) => Number.isInteger(value) && value > 0 ? value : fallback;

export function galleryDeviceLimits(state) {
    if (!state.socket.isConnected || !state.galleryCapabilities) return null;
    const info = state.galleryCapabilities;
    const width = positive(info.width, 32), height = positive(info.height, 8);
    return {
        frames: positive(info.frames, 28), pixels: positive(info.pixels, 1792),
        width, height, bitmapPixels: positive(info.bitmapPixels, width * height),
        messageBytes: positive(info.messageBytes, 12288),
    };
}

export function galleryMetadata(bmp) {
    const source = bmp.rgB565Array;
    const width = Number(bmp.sizeX), height = Number(bmp.sizeY);
    const cached = metadataCache.get(bmp);
    if (cached && cached.source === source && cached.width === width && cached.height === height) return cached;
    // API records contain comma-separated frame arrays, without an outer array.
    // Counting their brackets avoids allocating a JSON node for every pixel.
    let frames = 0, inside = false, afterFrame = false, valid = typeof source === 'string';
    if (valid) {
        for (let i = 0; i < source.length && valid; i++) {
            const c = source[i];
            if (c === '[') {
                if (inside || afterFrame) valid = false;
                else { inside = true; frames++; }
            } else if (c === ']') {
                if (!inside) valid = false;
                else { inside = false; afterFrame = true; }
            } else if (!inside && c === ',') {
                if (!afterFrame) valid = false;
                afterFrame = false;
            } else if (!inside && !/\s/.test(c)) valid = false;
        }
    }
    valid = valid && !inside && frames > 0 && Number.isInteger(width) && width > 0 && Number.isInteger(height) && height > 0;
    const result = Object.freeze({ source, width, height, frames, valid });
    metadataCache.set(bmp, result);
    return result;
}

export function galleryCompatibility(bmp, limits) {
    const meta = galleryMetadata(bmp);
    if (!meta.valid) return { ok: false, reason: 'Invalid bitmap format.' };
    if (!limits) return { ok: false, reason: 'Connect a device to check compatibility.' };
    if (meta.width > limits.width || meta.height > limits.height || meta.width * meta.height > limits.bitmapPixels) {
        return { ok: false, reason: `Too large for this device · ${meta.width}×${meta.height}, display ${limits.width}×${limits.height}.` };
    }
    // Preserve the existing Gallery envelope: single 8-wide images use animation.
    if (meta.frames > 1 || meta.width === 8) {
        const maximum = Math.min(limits.frames, Math.floor(limits.pixels / (meta.width * meta.height)));
        if (meta.frames > maximum) return {
            ok: false, reason: `Too large for this device · ${meta.frames} frames, maximum ${maximum} for ${meta.width}×${meta.height}.`,
        };
    }
    return { ok: true, reason: '' };
}

export function buildGalleryCommand(bmp, limits) {
    const status = galleryCompatibility(bmp, limits);
    if (!status.ok) throw new Error(status.reason);
    const meta = galleryMetadata(bmp);
    let frames;
    try { frames = JSON.parse(`[${meta.source.trim().replace(/,$/, '')}]`); }
    catch (error) { throw new Error('Invalid bitmap data. This image cannot be sent.'); }
    if (frames.length !== meta.frames || !frames.every(frame => Array.isArray(frame) &&
        frame.length === meta.width * meta.height && frame.every(pixel => Number.isInteger(pixel) && pixel >= 0 && pixel <= 65535))) {
        throw new Error('Invalid bitmap data. Frame dimensions and RGB565 pixels must match.');
    }
    const size = { width: meta.width, height: meta.height }, position = { x: 0, y: 0 };
    const command = { setScreen: meta.frames > 1 || meta.width === 8 ? {
        bitmapAnimation: { data: frames, animationDelay: 200, size, position },
    } : { bitmap: { data: frames[0], position, size } } };
    // All generated keys and validated numeric values are ASCII, so characters
    // equal UTF-8 wire bytes. Do not include Gallery names/user metadata.
    if (JSON.stringify(command).length > limits.messageBytes) {
        throw new Error(`Image message is too large for this device · maximum ${limits.messageBytes} bytes.`);
    }
    return command;
}

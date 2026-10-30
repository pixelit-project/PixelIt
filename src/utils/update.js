export function firmwareWarning(file, hardware, kind) {
    if (!file || !/\.bin$/i.test(file.name) || !file.size) return 'Select a non-empty .bin file.';
    if (/\.full-upgrade\.bin$/i.test(file.name)) return 'Full-upgrade images are for flashing via USB. Select the regular firmware .bin for this update.';
    const match = file.name.match(/^firmware_v.+_(ESP(?:32|8266)_.+)\.bin$/i);
    if (kind === 'filesystem' && match) return 'This is a firmware image, not a filesystem image.';
    if (!hardware) return 'Hardware could not be detected. Verify that this file is intended for your PixelIt.';
    if (match && match[1] !== hardware) return `File hardware: ${match[1]}. Connected PixelIt: ${hardware}.`;
    if (!match) return 'Hardware compatibility cannot be determined from this filename. Verify the file before continuing.';
    return '';
}

export function releaseAsset(release, hardware) {
    if (!release || !hardware || !Array.isArray(release.assets)) return null;
    return release.assets.find(asset => asset.name === `firmware_v${release.tag_name.replace(/^v/, '')}_${hardware}.bin`) || null;
}

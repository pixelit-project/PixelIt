const fs = require('fs');
const path = require('path');
const vm = require('vm');
const context = {};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../src/utils/galleryCompatibility.js'), 'utf8')
    .replace(/^export /gm, '') + '\nthis.api = { galleryDeviceLimits, galleryMetadata, galleryCompatibility, buildGalleryCommand };', context);
module.exports = context.api;

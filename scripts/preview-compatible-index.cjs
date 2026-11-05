// Generate a device index with the visibility controller and an already-published bundle.
// No application assets are rebuilt/replaced; source comes from public/preview-visibility.js.
const fs = require('fs');
const path = require('path');
const [input, output] = process.argv.slice(2);
if (!input || !output) throw new Error('Usage: node scripts/preview-compatible-index.cjs INPUT OUTPUT');
const source = fs.readFileSync(path.join(__dirname, '../public/preview-visibility.js'), 'utf8');
if (/<\/script/i.test(source)) throw new Error('Unsafe inline script termination');
let html = fs.readFileSync(input, 'utf8');
html = html.replace(/<script data-pixelit-preview-visibility>[\s\S]*?<\/script>/g, '');
if (!/<head(?:\s[^>]*)?>/i.test(html)) throw new Error('Missing head');
html = html.replace(/<head(?:\s[^>]*)?>/i, tag => tag + '<script data-pixelit-preview-visibility>' + source + '</script>');
fs.writeFileSync(output, html);

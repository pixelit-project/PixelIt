const fs = require('fs');
const path = require('path');
module.exports = {
  chainWebpack: config => {
    config.plugin('html').tap(args => {
      args[0].previewVisibilitySource = fs.readFileSync(path.join(__dirname, 'public/preview-visibility.js'), 'utf8');
      return args;
    });
  },
  transpileDependencies: [
    'vuetify'
  ],
  publicPath: process.env.NODE_ENV === 'production' ? 'https://pixelit-project.github.io/PixelIt/webui/' : '/'
}
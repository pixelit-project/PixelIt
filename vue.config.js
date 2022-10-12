module.exports = {
  transpileDependencies: [
    'vuetify'
  ],
  publicPath: process.env.NODE_ENV === 'production' ? 'https://pixelit-project.github.io/PixelIt/webui/' : '/'
}
<template>
    <l-map :style="style" :zoom="zoom" :center="center">
        <l-tile-layer :url="url" :attribution="attribution"></l-tile-layer>
        <v-marker-cluster>
            <l-marker v-for="(item, index) in coords" :key="'marker-' + index" :lat-lng="item"> </l-marker>
        </v-marker-cluster>
    </l-map>
</template>

<script>
import { LMap, LTileLayer, LMarker } from 'vue2-leaflet';
import { Icon } from 'leaflet';
import Vue2LeafletMarkerCluster from 'vue2-leaflet-markercluster';

delete Icon.Default.prototype._getIconUrl;
Icon.Default.mergeOptions({
    iconRetinaUrl: require('leaflet/dist/images/marker-icon-2x.png'),
    iconUrl: require('leaflet/dist/images/marker-icon.png'),
    shadowUrl: require('leaflet/dist/images/marker-shadow.png'),
});

export default {
    name: 'UserMap',
    props: {
        coords: {
            type: Array,
            required: true,
        },
        height: {
            type: String,
            required: true,
        },
    },
    components: {
        LMap,
        LTileLayer,
        LMarker,
        'v-marker-cluster': Vue2LeafletMarkerCluster,
    },
    data() {
        return {
            url: 'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
            attribution: '&copy; <a target="_blank" href="http://osm.org/copyright">OpenStreetMap</a> contributors',
            zoom: 2,
            center: [51.505, -0.159],
            style: `height: ${this.height}`,
        };
    },
};
</script>

<style>
@import '~leaflet.markercluster/dist/MarkerCluster.css';
@import '~leaflet.markercluster/dist/MarkerCluster.Default.css';
</style>

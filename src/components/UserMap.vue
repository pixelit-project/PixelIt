<template>
    <l-map :style="style" :zoom="zoom" :center="center">
        <l-tile-layer :url="url" :attribution="attribution"></l-tile-layer>
        <v-marker-cluster :options="options">
            <l-marker v-for="(item, index) in coords" :key="'marker-' + index" :lat-lng="item" :icon="icon"> </l-marker>
        </v-marker-cluster>
    </l-map>
</template>

<script>
import L from 'leaflet';
import { Icon } from 'leaflet';
import { LMap, LTileLayer, LMarker } from 'vue2-leaflet';
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
        mapZoom: {
            type: Number,
            required: false,            
        },
    },
    components: {
        LMap,
        LTileLayer,
        LMarker,
        'v-marker-cluster': Vue2LeafletMarkerCluster,
    },
    computed: {
        dynamicSize() {
            return [this.iconSize, this.iconSize * 1.15];
        },
        dynamicAnchor() {
            return [this.iconSize / 2, this.iconSize * 1.15];
        },
    },
    data() {
        return {
            url: 'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
            attribution: '&copy; <a target="_blank" href="http://osm.org/copyright">OpenStreetMap</a> contributors',
            zoom: this.mapZoom || 2,
            center: [51.505, -0.159],
            style: `height: ${this.height}`,
            icon: L.icon({
                iconUrl:
                    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABQAAAAgCAYAAAASYli2AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAARtSURBVEhLrVW7bxxlEP/t67z23T58trHvFF0R2wU2FCHGCRYpqCxFfpS4pIEqNFSACTRJSjrCP0BBgRApU6FENCBFBhHFhSufrXvlbO/L5/Pe3i0zn/cOG78C4jf6aWZn5pvvtbMrIcHy8vL9tbW1zw3DEM++7yOfy6FULuMy31vXrj346dGjVeHoYm5u7j6p2LIsQbYLhYLQl/mSsQLS0uLiVzu7u8q4cfO9nRfau2kjhQkKdPwQcS7GVnkLuqGL5KbfhJ2z4ZQdKIaCP/A7HN/F9NTUL57v/zwyMgKJZywWi1gsrOJ68R4GrBY+osGWq+F54TnWi+uQLVkU7LgdqAUVUTFCYAX4lKTm1kA1wDVYy3wWtHT0Gxo0KqaZESJii+y20UbbIpoJ2U58sRlj2M5iNDsM27REDa6liqnPwa8kD0gyJIyAJEdSJrmivYYf3/kE6ZaMb3af4tutosiR+ZZc18WB30KLttnyVKhEjeyG38Cmu4ltb1uQ7ZJfEroaVDAWp3ElNqEcdkQNrqXm83lROZtPYwAN2nqEQ3pu0Jb1vI4CSfcV4W1188eGcmjaNHGkIt22xflxTC2VSuJAd7GPRnGARrXQRwMGaIVNEo5xIQavgsG+VDOG7kQYaEnYf+kIH0NOaRr6UimomkwPHcFOQkmTRCyVUNhJvka6o0joqBJkVenFZJNmH8xm6XXpQyobQrNbiIgh25YmYrZtC7LdzTdtC1G/jJDYl+nvxeR6vY5KpQKvfoBmRcdhjQoTdbLDeihitVpNkO1u/s7LOlJBG7rfxoET9GIq9yYjmzt9KVdzV/EBiW7wqVKnWIewchZckoyt4zv/GeJmiHJ/8+hSqJbKjX7epVwluV28fbJTSESnuNQpmyc7hXFhp0QGnaUVIjQTsm0cac4xzKOx3Rqv0Cm/4R6JkXSKf6xTMkiDphP+47iwUw78A2y72yh5ZUG2q35V6LJXget5vQ75u1MuuBQ913e6U5L8s3yXXkqTSnOMBzGOd8pZPoacTqfFbHpag0qrUzMR2sSIbCWtiFgmQ98bItvd/LN8rGVFUaCqKhncQtRuaoxYkGzycUzlnCSvm3+cXR9r2aOD3dvbQ8Oj12EvhZZDKyVqZEdeJGIObYvJdjffcRzB4z7W0srKyvc71DKT5q03w42xqf6Mgmk6C4naamN0A4+rj5GhrTCC/X2Mjo6iWq2K7QpfEGBycvKF73l/0j+l9xf9X/56cRxLRz1FoANt25Z1aJlmaJomJ4uDZk3Pgv/wxZzLY2i1ba4hSVLcK/j2zMzD+fn5mYWFhUVZlp3EfS44h3N5zM0bNx4m7tP48u7dAn37amReuGXO4VyyT+BUL2ez2cYb09M/+EFgDA0NXSfX67xNBm91YmJifXBw8BltORgeHm6IwKtidnb2a1InVpj4zkXvDM8C/Tdk/ndo9NIKsk2+JPzv8fTJk6GP79z5jJo+Hh8fj79YXX2ffUn4v4Fu8kNSMb1W8fLS0q0j73kA/gKWLYMBPFTNLQAAAABJRU5ErkJggg==',
                iconSize: [20, 30],
                iconAnchor: [8, 32],
            }),
            options: { maxClusterRadius: 33 },
        };
    },
};
</script>

<style>
@import '~leaflet.markercluster/dist/MarkerCluster.css';
@import '~leaflet.markercluster/dist/MarkerCluster.Default.css';
</style>

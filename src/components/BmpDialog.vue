<template>
    <v-dialog v-model="dialog" width="500">
        <template v-slot:activator="{ on, attrs }">
            <v-btn icon v-bind="attrs" v-on="on" class="float-left" title="More information" aria-label="More information">
                <v-icon>mdi-information-outline</v-icon>
            </v-btn>
        </template>
        <v-card>
            <v-card-title>
                <div class="text-center">
                    {{ bmp.name }}
                </div>
            </v-card-title>
            <v-divider></v-divider>

            <v-card-text>
                <BmpCanvas class="float-right" :bmp="bmp" height="80" :width="bmp.sizeX == 8 ? '80' : '160'" />
                <p></p>
                Icon-ID: {{ bmp.id }}<br />
                Added by: {{ bmp.username }}<br />
                Added on: {{ bmp.dateTime.split('T')[0] }}<br />
                Animated: {{ bmp.animated ? 'Yes' : 'No' }}<br />
                <br />
                <v-textarea filled outlined :value="bmp.rgB565Array" readonly rows="8"></v-textarea>
            </v-card-text>
            <v-divider></v-divider>
            <v-card-actions>
                <v-spacer></v-spacer>
                <v-btn color="primary" text @click="dialog = false"> close </v-btn>
            </v-card-actions>
        </v-card>
    </v-dialog>
</template>

<script>
import BmpCanvas from './BmpCanvas';
export default {
    props: {
        bmp: {
            type: Object,
            required: true,
        },
    },
    components: {
        BmpCanvas,
    },
    watch: {
        dialog(value) {
            this.$emit('dialog-change', value);
        },
    },
    data: function () {
        return {
            dialog: false,
        };
    },
};
</script>

<template>    
    <v-dialog v-model="dialog" persistent max-width="450">        
        <template v-slot:activator="{ on, attrs }">
            <v-btn :color="color" dark v-bind="attrs" v-on="on" :disabled="!condition">
                <v-icon left>{{ icon }}</v-icon>
                {{ text }}
            </v-btn>
        </template>
        <v-card>
            <v-form v-model="isValid">
                <v-card-title class="text-h5">
                    Save Bitmap?
                </v-card-title>
                <v-card-text>
                    <v-text-field  ref="field" v-model="bitmapName"  label="Bitmap Name" :rules="[rules.required, rules.notStartsWithSpace]"></v-text-field>
                    <v-text-field v-model="userName" @change="changeUserName" label="Your Username" :rules="[rules.notStartsWithSpace]"></v-text-field>
                </v-card-text>
                <v-card-actions>
                    <v-spacer></v-spacer>
                    <v-btn color="green darken-1" text @click="agreeWraper" :loading="saving" :disabled="(saving || (!isValid) || (getSizeX == 32 && isAnimated == true))">
                        Save into Database              
                    </v-btn>
                    <v-btn color="red darken-1" text :disabled="saving" @click="disagreeWraper">
                        Abort
                    </v-btn>
                </v-card-actions>
            </v-form>
        </v-card>
    </v-dialog>
</template>

<script>
import { notify } from '../utils/feedback';
export default {
    data() {        
        return {
            dialog: false,
            saving: false,
            isValid: false,
            bitmapName: ' ',
            userName: this.$cookies.get('userNameForUpload') || '',                        
        };        
    },
    props: {
        color: {
            type: String,
            required: false,
        },
        icon: {
            type: String,
            required: false,
        },
        text: {
            type: String,
            required: false,
        },
        data: {
            type: String,
            required: false,
        },
        pixelMode:{
            type: Number,
            required: false,
        },
        condition: {
            type: Boolean,
            required: true,
        },
    },
    computed: {
        rules() {
            return this.$store.state.rules;
        },
        cleanedData(){
            return this.data.replaceAll('\n', '').replaceAll(' ', '');
        },
        isAnimated(){
            return this.cleanedData.includes('],[');
        },
        getSizeX(){
            return this.pixelMode == 0 ? 8 : 32;
        }
    },
    methods: {
        disagreeWraper() {
            this.dialog = false;
        },
        async agreeWraper() {
            if (this.saving || !this.isValid) return;
            this.saving = true;
            try {
                const response = await fetch(`${this.$apiServerBaseURL}/SaveBitmap`, {
                    method: 'POST',
                    headers: { Accept: 'application/json', 'Content-Type': 'application/json', Client: this.$client },
                    body: JSON.stringify({
                        rgb565array: this.cleanedData,
                        userName: this.userName || 'Not specified',
                        name: this.bitmapName,
                        sizeX: this.getSizeX,
                        sizeY: 8,
                        animated: this.isAnimated,
                    }),
                });
                if (!response.ok) throw new Error(`HTTP ${response.status}`);
                this.dialog = false;
                notify('Bitmap upload accepted by the gallery.', 'success');
            } catch (error) {
                notify('Bitmap upload failed. Your drawing is still available; please try again.', 'error');
            } finally { this.saving = false; }
        },
        changeUserName(){
            this.$cookies.set('userNameForUpload', this.userName)
        }
    }, 
    
};
</script>

<template>
    <v-dialog v-model="dialog" persistent max-width="450">
        <template v-slot:activator="{ on, attrs }">
            <v-btn :color="color" dark v-bind="attrs" v-on="on" :disabled="!condition">
                <v-icon left>{{ icon }}</v-icon>
                {{ text }}
            </v-btn>
        </template>
        <v-card>
            <v-card-title class="text-h5">
                {{ title }}
            </v-card-title>
            <v-card-text v-for="cText in cardText" :key="cText">{{ cText }}</v-card-text>
            <v-card-actions>
                <v-spacer></v-spacer>
                <v-btn color="green darken-1" text @click="disagreeWraper, (dialog = false)">
                    {{ disagreeText }}
                </v-btn>
                <v-btn color="red darken-1" text @click="agreeWraper">
                    {{ agreeText }}
                </v-btn>
            </v-card-actions>
        </v-card>
    </v-dialog>
</template>

<script>
export default {
    data() {
        return {
            dialog: false,
        };
    },
    props: {
        condition: {
            type: Boolean,
            required: true,
        },
        text: {
            type: String,
            required: false,
        },
        cardText: {
            type: Array,
            required: false,
        },
        agreeText: {
            type: String,
            required: false,
        },
        disagreeText: {
            type: String,
            required: false,
        },
        title: {
            type: String,
            required: false,
        },
        color: {
            type: String,
            required: false,
        },
        onclickAgree: {
            type: Function,
            required: false,
        },
        onclickDisagree: {
            type: Function,
            required: false,
        },
        icon: {
            type: String,
            required: false,
        },
    },
    methods: {
        save() {
            this.$socket.sendObj({ setConfig: this.config });
            setTimeout(() => {
                this.$socket.close();
            }, 3000);
        },
        disagreeWraper() {
            if (this.onclickDisagree) {
                this.onclickDisagree();
            }
            this.dialog = false;
        },
        agreeWraper() {
            if (this.onclickAgree) {
                this.onclickAgree();
            }
            this.dialog = false;
        },
    },
};
</script>

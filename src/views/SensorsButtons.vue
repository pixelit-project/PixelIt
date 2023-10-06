<template>
    <v-container class="options">
        <v-row>
            <v-col cols="12" lg="12" class="text-center">
                <ButtonCondition color="success" :condition="isValid && sockedIsConnected" text="Save config" :onclick="save" icon="mdi-content-save" />
            </v-col>
        </v-row>
        <v-form v-model="isValid">
            <v-row>
                <v-col cols="12" lg="4">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Sensor units and offsets</h2>
                        </v-card-title>
                        <hr />
                        <br />
                        <v-select :items="temperatureUnits" v-model="config.temperatureUnit" label="Temperature unit"></v-select>
                        <v-text-field v-model="config.temperatureOffset" type="number" label="Temperature sensor offset" :rules="[rules.required]"></v-text-field>
                        <v-text-field v-model="config.humidityOffset" type="number" label="Humidity sensor offset" :rules="[rules.required]"></v-text-field>
                        <v-text-field v-model="config.pressureOffset" type="number" label="Pressure sensor offset" :rules="[rules.required]"></v-text-field>
                        <v-text-field v-model="config.gasOffset" type="number" label="Gas sensor offset" :rules="[rules.required]"></v-text-field>
                        <v-text-field v-model="config.luxOffset" type="number" label="Lux sensor offset" :rules="[rules.required]"></v-text-field>
                        <v-text-field v-model="config.ldrSmoothing" type="number" label="Number of historic LDR readings to be used for linear smoothing (LDR only)" hint="Enter any value when using BH1750" :rules="[rules.required, rules.min0]"></v-text-field>
                    </v-card>
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Sensor hardware</h2>
                        </v-card-title>
                        <hr />
                        <br />
                        <h3>I²C sensors</h3>
                        <v-card-text>If you use BH1750, BME280 or BME680 sensors, these need two pins to communicate. You can use both Lux and Temperature sensors at the same time: just connect them in parallel. If there are no sensors at all, just select any two open pins. </v-card-text>
                        <v-select :items="config.isESP8266 ? pinsESP8266 : pinsESP32" v-model="config.SCLPin" type="number" label="SCL pin" hint="Pick any value when using no I²C sensors"></v-select>
                        <v-select :items="config.isESP8266 ? pinsESP8266 : pinsESP32" v-model="config.SDAPin" type="number" label="SDA pin" hint="Pick any value when using no I²C sensors"></v-select>
                        <br />
                        <h3>OneWire sensors</h3>
                        <v-card-text>If you use OneWire sensors like DHT22, they need one pin to communicate. If there are no I²C sensors (see above), you can re-use one of the pins above. If there is no OneWire sensor, just select any open pin. </v-card-text>
                        <v-select :items="config.isESP8266 ? pinsESP8266 : pinsESP32" v-model="config.onewirePin" type="number" label="DHT sensor pin" hint="Pick any value when using no OneWire sensors"></v-select>
                        <h3>LDR</h3>
                        <v-card-text>If you use no BH1750, but an LDR (light dependend resistor) connected to the A0 pin, you can define its type and the pulldown resistor here. Select any values if there is no LDR.</v-card-text>
                        <v-select :items="ldrDevices" v-model="config.ldrDevice" type="number" label="Lux sensor type" hint="Pick any value when using BH1750 or no lux sensor at all"></v-select>
                        <v-text-field v-model="config.ldrPulldown" type="number" label="Value of pulldown resistor for LDR" suffix="Ohm" hint="Enter any value when using BH1750 or no lux sensor at all" :rules="[rules.required]"></v-text-field>
                    </v-card>
                </v-col>
                <v-col cols="12" lg="4">
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Button hardware</h2>
                        </v-card-title>
                        <hr />
                        <br />
                        <v-switch v-model="config.btn0Enabled" label="Left button enabled" dense hide-details></v-switch>
                        <v-select :items="config.isESP8266 ? pinsESP8266 : pinsESP32" v-model="config.btn0Pin" type="number" label="Pin for left button" :disabled="!config.btn0Enabled"></v-select>
                        <v-select :items="btnLowHigh" v-model="config.btn0PressedLevel" type="number" label="Left button signal type" :disabled="!config.btn0Enabled"></v-select>
                        <br />
                        <v-switch v-model="config.btn1Enabled" label="Middle button enabled" dense hide-details></v-switch>
                        <v-select :items="config.isESP8266 ? pinsESP8266 : pinsESP32" v-model="config.btn1Pin" type="number" label="Pin for middle button" :disabled="!config.btn1Enabled"></v-select>
                        <v-select :items="btnLowHigh" v-model="config.btn1PressedLevel" type="number" label="Middle button signal type" :disabled="!config.btn1Enabled"></v-select>
                        <br />
                        <v-switch v-model="config.btn2Enabled" label="Right button enabled" dense hide-details></v-switch>
                        <v-select :items="config.isESP8266 ? pinsESP8266 : pinsESP32" v-model="config.btn2Pin" type="number" label="Pin for right button" :disabled="!config.btn2Enabled"></v-select>
                        <v-select :items="btnLowHigh" v-model="config.btn2PressedLevel" type="number" label="Right button signal type" :disabled="!config.btn2Enabled"></v-select>
                    </v-card>
                    <br />
                    <v-card class="pa-2" elevation="4">
                        <v-card-title>
                            <h2>Button actions</h2>
                        </v-card-title>
                        <hr />
                        <br />
                        <v-select :items="btnActions" v-model="config.btn0Action" type="number" label="Left button action" :disabled="!config.btn0Enabled"></v-select>
                        <v-select :items="btnActions" v-model="config.btn1Action" type="number" label="Middle button action" :disabled="!config.btn1Enabled"></v-select>
                        <v-select :items="btnActions" v-model="config.btn2Action" type="number" label="Right button action" :disabled="!config.btn2Enabled"></v-select>
                    </v-card>
                </v-col>
            </v-row>
        </v-form>
    </v-container>
</template>

<script>
import ButtonCondition from '../components/ButtonCondition';
export default {
    name: 'SensorsButtons',
    data: () => ({
        isValid: true,
    }),
    components: {
        ButtonCondition,
    },
    computed: {
        rules() {
            return this.$store.state.rules;
        },
        config() {
            return this.$store.state.configData;
        },
        sockedIsConnected() {
            return this.$store.state.socket.isConnected;
        },
        temperatureUnits() {
            return this.$store.state.temperatureUnits;
        },
        ldrDevices() {
            return this.$store.state.ldrDevices;
        },
        isESP8266() {
            return this.$store.state.config.isESP8266;
        },
        btnLowHigh() {
            return this.$store.state.btnLowHigh;
        },
        btnActions() {
            return this.$store.state.btnActions;
        },
        pinsESP32() {
            return this.$store.state.pinsESP32;
        },
        pinsESP8266() {
            return this.$store.state.pinsESP8266;
        },
    },
    methods: {
        save() {
            this.$socket.sendObj({ setConfig: this.config });
            setTimeout(() => {
                this.$socket.close();
            }, 3000);
        },
    },
};
</script>

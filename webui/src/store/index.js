import Vue from "vue";
import Vuex from "vuex";

let pingInterval;

Vue.use(Vuex);

export default new Vuex.Store({
    state: {
        socket: {
            isConnected: false,
            reconnectError: false,
        },
        testarea: {
            brightness: 0,
            json: JSON.stringify({
                text: {
                    textString: "Pixel it 😀",
                    scrollText: "auto",
                    bigFont: false,
                    centerText: false,
                    scrollTextDelay: 40,
                    hexColor: "#FFFFFF",
                    position: {
                        x: 0,
                        y: 1,
                    },
                },
            },
                null,
                4
            ),
            text: "€ ← ↑ → ↓ ★ 📁 📄 ♥ ↧ 🚗 😀",
            image: "[33808,0,0,0,0,0,0,33808,43680,33808,0,0,0,0,33808,43680,43680,33808,44373,44373,44373,44373,33808,43680,33808,65535,65535,44373,44373,65535,65535,33808,33808,65535,0,44373,44373,65535,0,33808,33808,65535,0,33808,33808,65535,0,33808,0,33808,33808,43680,43680,33808,33808,0,0,0,33808,33808,33808,33808,0,0],[33808,0,0,0,0,0,0,33808,43680,33808,0,0,0,0,33808,43680,43680,33808,44373,44373,44373,44373,33808,43680,33808,65535,65535,44373,44373,65535,65535,33808,33808,0,65535,44373,44373,0,65535,33808,33808,0,65535,33808,33808,0,65535,33808,0,33808,33808,43680,43680,33808,33808,0,0,0,33808,33808,33808,33808,0,0],[33808,0,0,0,0,0,0,33808,43680,33808,0,0,0,0,33808,43680,43680,33808,44373,44373,44373,44373,33808,43680,33808,65535,65535,44373,44373,65535,65535,33808,33808,65535,0,44373,44373,65535,0,33808,33808,65535,0,33808,33808,65535,0,33808,0,33808,33808,43680,43680,33808,33808,0,0,0,33808,33808,33808,33808,0,0],[33808,0,0,0,0,0,0,33808,43680,33808,0,0,0,0,33808,43680,43680,33808,44373,44373,44373,44373,33808,43680,33808,44373,44373,44373,44373,44373,44373,33808,33808,44373,44373,44373,44373,44373,44373,33808,33808,0,0,33808,33808,0,0,33808,0,33808,33808,43680,43680,33808,33808,0,0,0,33808,33808,33808,33808,0,0]",
        },
        newVersionAvailable: false,
        version: null,
        gitVersion: null,
        gitDownloadUrl: null,
        gitReleases: [],
        logData: [],
        sensorData: [],
        buttonData: [],
        sysInfoData: [],
        configData: {},
        rules: {
            required: (value) => !!value || value == "0" || "Required.",
            max20Chars: (value) => value.length <= 20 || "Max 20 characters",
            email: (value) => {
                const pattern = /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
                return pattern.test(value) || "Invalid e-mail.";
            },
            min0: (value) => value >= 0 || "Must be greater than or equal to 0",
            max255: (value) => value <= 255 || "Must be less than or equal to 255",
            minMinus12: (value) => value >= -12 || "Must be greater than or equal to -12",
            max14: (value) => value <= 14 || "Must be less than or equal to 14",
            portRange: (value) => (value > 0 && value <= 65535) || "Must be between 1 and 65535",
            volumeRange: (value) => (value > 0 && value <= 30) || "Must be between 1 and 30",
            noDecimals: (value) => (value % 1 === 0) || "No decimals allowed",
            // Can not be solved like this, we do not have access to the store.
            //   noPinDuplicates: value => ((   //dirty code, but creating an array and running a loop seems to be over-engineering
            //       this.config.DFPRXpin!=this.config.DFPTXpin && this.config.DFPRXpin!=this.config.BMESDAPin && this.config.DFPRXpin!=this.config.BMESCLPin && this.config.DFPRXpin!=this.config.DHTPin
            //    && this.config.DFPTXpin!=this.config.BMESDAPin && this.config.DFPTXpin!=this.config.BMESCLPin && this.config.DFPTXpin!=this.config.DHTPin
            //    && this.config.BMESDAPin!=this.config.BMESCLPin && this.config.BMESDAPin!=this.config.DHTPin
            //   //DHT pin and BME-SCL-pin may be identical!
            //  ) || 'Pin assignment must be unique'),
        },
        navLinks: [{
            title: "Dashboard",
            icon: "mdi-memory",
            page: "/"
        },
        {
            title: "Options",
            icon: "mdi-tune-vertical",
            page: "/options"
        },
        {
            title: "Sensors & Buttons",
            icon: "mdi-gesture-tap-button",
            page: "/sensorsbuttons"
        },
        {
            title: "Test Area",
            icon: "mdi-cube-outline",
            page: "/testarea"
        },
        {
            title: "Update",
            icon: "mdi-tray-arrow-up",
            page: "/update"
        },
        {
            title: "Pixel Gallery",
            icon: "mdi-image-outline",
            page: "/gallery"
        },
        {
            title: "Pixel Creator",
            icon: "mdi-pencil-box-outline",
            url: "https://pixelit.bastelbunker.de/PixelCreator",
            target: "_blank"
        },
        // {
        //     title: "Pixel Creator",
        //     icon: "mdi-pencil-box-outline",
        //     page: "/creator"
        // },
        {
            title: "Forum",
            icon: "mdi-forum-outline",
            url: "https://github.com/pixelit-project/PixelIt/discussions",
            target: "_blank"
        },
        {
            title: "Blog",
            icon: "mdi-post-outline",
            url: "https://www.bastelbunker.de/pixel-it/",
            target: "_blank"
        },
        {
            title: "Documentation",
            icon: "mdi-book-open-page-variant-outline",
            url: "https://pixelit-project.github.io/",
            target: "_blank"
        },
        {
            title: "GitHub",
            icon: "mdi-github",
            url: "https://github.com/pixelit-project/PixelIt",
            target: "_blank"
        },
        ],
        matrixTypes: [{
            text: "Type 1 - Colum major",
            value: 1
        },
        {
            text: "Type 2 - Row major",
            value: 2
        },
        {
            text: "Type 3 - Tiled 4x 8x8 CJMCU (Column major)",
            value: 3
        },
        {
            text: "Type 5 - Tiled 4x 8x8 CJMCU (Row major)",
            value: 5
        },
        {
            text: "Type 4 - MicroMatrix ᵇʸ ᶠᵒᵒʳˢᶜʰᵗᵇᵃʳ",
            value: 4
        },
        ],
        matrixCorrection: [{
            text: "Default",
            value: "default"
        },
        {
            text: "Typical SMD 5050",
            value: "typicalsmd5050"
        },
        {
            text: "Typical 8mm Pixel",
            value: "typical8mmpixel"
        },
        {
            text: "Tungsten 40W",
            value: "tungsten40w"
        },
        {
            text: "Tungsten 100W",
            value: "tungsten100w"
        },
        {
            text: "Halogen",
            value: "halogen"
        },
        {
            text: "Carbon Arc",
            value: "carbonarc"
        },
        {
            text: "High Noon Sun",
            value: "highnoonsun"
        },
        {
            text: "Direct Sunlight",
            value: "directsunlight"
        },
        {
            text: "Overcast Sky",
            value: "overcastsky"
        },
        {
            text: "Clear Blue Sky",
            value: "clearbluesky"
        },
        {
            text: "Warm Fluorescent",
            value: "warmfluorescent"
        },
        {
            text: "Standard Fluorescent",
            value: "standardfluorescent"
        },
        {
            text: "Cool White Fluorescent",
            value: "coolwhitefluorescent"
        },
        {
            text: "Full Spectrum Fluorescent",
            value: "fullspectrumfluorescent"
        },
        {
            text: "Grow Light Fluorescent",
            value: "growlightfluorescent"
        },
        {
            text: "Black Light Fluorescent",
            value: "blacklightfluorescent"
        },
        {
            text: "Mercury Vapor",
            value: "mercuryvapor"
        },
        {
            text: "Sodium Vapor",
            value: "sodiumvapor"
        },
        {
            text: "Metal Halide",
            value: "metalhalide"
        },
        {
            text: "High Pressure Sodium",
            value: "highpressuresodium"
        },
        ],
        // matrixColorOrder:[
        //     { text: "RGB - Red, Green, Blue", value: 0 },
        //     { text: "RBG - Red, Blue, Green", value: 1 },
        //     { text: "GRB - Green, Red, Blue", value: 2 },
        //     { text: "GBR - Green, Blue, Red", value: 3 },
        //     { text: "BRG - Blue, Red, Green", value: 4 },
        //     { text: "BGR - Blue, Green, Red", value: 5 },
        // ],
        autoFallbackAnimation: [{
            text: "None",
            value: 0
        },
        {
            text: "Fade",
            value: 1
        },
        {
            text: "Colored Bar Wipe",
            value: 2
        },
        {
            text: "ZigZag Wipe",
            value: 3
        },
        {
            text: "Random Wipe",
            value: 4
        },
        ],
        temperatureUnits: [{
            text: "Celsius °C",
            value: 0
        },
        {
            text: "Fahrenheit °F",
            value: 1
        },
        ],
        ldrDevices: [{
            text: "GL5516",
            value: "GL5516"
        },
        {
            text: "GL5528",
            value: "GL5528"
        },
        {
            text: "GL5537_1",
            value: "GL5537_1"
        },
        {
            text: "GL5537_2",
            value: "GL5537_2"
        },
        {
            text: "GL5539",
            value: "GL5539"
        },
        {
            text: "GL5549",
            value: "GL5549"
        },
        ],
        pinsESP8266: [{
            text: "Pin D0",
            value: "Pin_D0"
        },
        {
            text: "Pin D1",
            value: "Pin_D1"
        },
        {
            text: "Pin D3",
            value: "Pin_D3"
        },
        {
            text: "Pin D4",
            value: "Pin_D4"
        },
        {
            text: "Pin D5",
            value: "Pin_D5"
        },
        {
            text: "Pin D6",
            value: "Pin_D6"
        },
        {
            text: "Pin D7",
            value: "Pin_D7"
        },
        {
            text: "Pin D8",
            value: "Pin_D8"
        },
        ],
        btnLowHigh: [{
            text: "Active low",
            value: 0
        },
        {
            text: "Active high",
            value: 1
        },
        ],
        btnActions: [{
            text: "None (only send to MQTT and API)",
            value: 0
        },
        {
            text: "Go to Clock",
            value: 1
        },
        {
            text: "Toggle Sleep Mode",
            value: 2
        },
        {
            text: "MP3: Toggle Play/Pause",
            value: 3
        },
        {
            text: "MP3: Previous track",
            value: 4
        },
        {
            text: "MP3: Next track",
            value: 5
        },
        ],
        bmpsFromAPI: [],
        pixelCreatorPixel: {},
        telemetryData: '',
        userMapData: []
    },
    mutations: {
        SOCKET_ONOPEN(state, event) {
            Vue.prototype.$socket = event.currentTarget;
            state.socket.isConnected = true;
            // Send Ping!?
            pingInterval = setInterval(() => {
                Vue.prototype.$socket.send(0x9);
            }, 1000);
        },
        SOCKET_ONCLOSE(state) {
            state.socket.isConnected = false;
            clearInterval(pingInterval);
        },
        SOCKET_ONERROR(state, event) {
            console.error(state, event);
        },
        // default handler called for all methods
        SOCKET_ONMESSAGE(state, message) {
            console.log(state);
            console.log(message);
            // Log
            if (message.log) {
                addToLogData(message.log, state);
            }
            // Sensor
            if (message.sensor) {
                addToSensorData(message.sensor, state);
            }
            // Buttons
            if (message.buttons) {
                addToButtonData(message.buttons, state);
            }
            // Config
            if (message.config) {
                addToConfigData(message.config, state);
            }
            // SystemInfo
            if (message.sysinfo) {
                addToSysInfoData(message.sysinfo, state);
            }
            // SystemInfo
            if (message.telemetry) {
                addToTelemetryData(message.telemetry, state);
            }
        },
        // mutations for reconnect methods
        SOCKET_RECONNECT(state, count) {
            console.info(state, count);
        },
        SOKE(state) {
            state.socket.reconnectError = true;
        },
        SOCKET_RECONNECT_ERROR(state) {
            state.socket.reconnectError = true;
        },
    },
    actions: {},
    modules: {},
});

function addToLogData(obj, state) {
    state.logData.unshift(`[${obj.timeStamp}] ${obj.function}: ${obj.message}`);
    if (state.logData.length > 100) {
        state.logData = state.logData.slice(0, 100);
    }
}

function addToSensorData(obj, state) {
    for (const key in obj) {
        const oldEntry = state.sensorData.find((x) => x.name == getDisplayName(key));
        if (oldEntry) {
            oldEntry.value = getDisplayValue(key, obj[key]);
        } else {
            state.sensorData.push({
                name: getDisplayName(key),
                value: getDisplayValue(key, obj[key])
            });
        }
    }
}

function addToButtonData(obj, state) {
    for (const key in obj) {
        const oldEntry = state.buttonData.find((x) => x.name == getDisplayName(key));
        if (oldEntry) {
            oldEntry.value = getDisplayValue(key, obj[key]);
        } else {
            state.buttonData.push({
                name: getDisplayName(key),
                value: getDisplayValue(key, obj[key])
            });
        }
    }
}

function addToConfigData(obj, state) {
    state.configData = obj;
    state.testarea.brightness = obj.matrixBrightness;
}

function addToTelemetryData(obj, state) {
    state.telemetryData = JSON.stringify(obj, null, 4);
}

function addToSysInfoData(obj, state) {
    for (const key in obj) {
        const oldEntry = state.sysInfoData.find((x) => x.name === getDisplayName(key));
        if (oldEntry) {
            oldEntry.value = getDisplayValue(key, obj[key]);
        } else {
            state.sysInfoData.push({
                name: getDisplayName(key),
                value: getDisplayValue(key, obj[key])
            });
        }
        if (key === "pixelitVersion") {
            state.version = obj[key];
        }
    }
}

function getDisplayName(key) {
    switch (key) {
        case "lux":
            key = "Luminance";
            break;
        case "temperature":
            key = "Temperature";
            break;
        case "humidity":
            key = "Humidity";
            break;
        case "gas":
            key = "Gas";
            break;
        case "pressure":
            key = "Pressure";
            break;
        case "pixelitVersion":
            key = "PixelIt version";
            break;
        case "hostname":
            key = "Hostname";
            break;
        case "note":
            key = "Note";
            break;
        case "sketchSize":
            key = "Sketch size";
            break;
        case "freeSketchSpace":
            key = "Free sketch space";
            break;
        case "wifiRSSI":
            key = "Wifi RSSI";
            break;
        case "wifiQuality":
            key = "Wifi quality";
            break;
        case "wifiSSID":
            key = "Wifi SSID";
            break;
        case "wifiBSSID":
            key = "Wifi BSSID";
            break;
        case "ipAddress":
            key = "IP-Address";
            break;
        case "freeHeap":
            key = "Free heap";
            break;
        case "chipID":
            key = "ChipID";
            break;
        case "cpuFreqMHz":
            key = "CPU freq.";
            break;
        case "sleepMode":
            key = "Sleep mode";
            break;
        case "currentMatrixBrightness":
            key = "Current Brightness";
            break;
        case "leftButton":
            key = "Left button";
            break;
        case "middleButton":
            key = "Middle button";
            break;
        case "rightButton":
            key = "Right button";
            break;
    }
    return key;
}

function getDisplayValue(key, value) {
    switch (key) {
        case "lux":
            if (typeof value == "number") {
                value = value.toFixed(3) + " lux";
            }
            break;
        case "note":
            if (!value.trim()) {
                value = "---";
            }
            break;
        case "sketchSize":
        case "freeSketchSpace":
        case "freeHeap":
            value = humanFileSize(value, true);
            break;
        case "wifiRSSI":
            value += " dBm";
            break;
        case "wifiQuality":
            value += " %";
            break;
        case "cpuFreqMHz":
            value += " MHz";
            break;
        case "sleepMode":
            value = value ? "On" : "Off";
            break;
        case "temperature":
            if (typeof value == "number") {
                value = Math.round(value * 10) / 10 + " °C";
            }
            break;
        case "humidity":
            if (typeof value == "number") {
                value = Math.round(value) + " %";
            }
            break;
        case "pressure":
            if (typeof value == "number") {
                value = Math.round(value) + " hPa";
            }
            break;
        case "gas":
            if (typeof value == "number") {
                value = Math.round(value) + " kOhm";
            }
            break;
    }
    return value;
}

function humanFileSize(bytes, si = false, dp = 1) {
    const thresh = si ? 1000 : 1024;

    if (Math.abs(bytes) < thresh) {
        return bytes + " B";
    }

    const units = si ? ["kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"] : ["KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"];
    let u = -1;
    const r = 10 ** dp;

    do {
        bytes /= thresh;
        ++u;
    } while (Math.round(Math.abs(bytes) * r) / r >= thresh && u < units.length - 1);

    return bytes.toFixed(dp) + " " + units[u];
}
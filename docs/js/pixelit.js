var ipAddress = $(location).attr('hostname');
var pageName = 'dash';
var devMode = false;
var timeleft;
var rebootTimer;
var json;
var titlechanged = false;
let currentGitRelease

if (ipAddress.includes('localhost')) {
    devMode = true;
}

$(function () {
    // Akive Menu Button select 
    $('.nav-link').click(function () {
        $('.nav-link').removeClass('active');
        $(this).addClass('active');
    })
    ChangePage("dash");
});

var connection = null;

function capitalizeFirstLetter(string) {
    return string.charAt(0).toUpperCase() + string.slice(1);
}

function connectionStart() {
    if (connection != null && connection.readyState != WebSocket.CLOSED) {
        connection.close();
    }

    var wsServer = ipAddress;
    // Dev Option
    if (devMode) {
        wsServer = '172.30.100.218'
    }
    connection = new WebSocket('ws://' + wsServer + ':81/' + pageName);
    connection.onopen = function () {
        $("#connectionStatus").html("Online");
        $("#connectionStatus").removeClass("text-danger");
        $("#connectionStatus").addClass("text-success");

        if (pageName == 'setConfig') {
            connection.send(json);
            connection.close();
        }

        //KeepAlive();
    }
    connection.onclose = function (e) {
        // Debug
        console.log('WebSocket connection close');
        $("#connectionStatus").html("Offline");
        $("#connectionStatus").removeClass("text-success");
        $("#connectionStatus").addClass("text-danger");
        /*
        if (pageName != 'config') {
            setTimeout(function () {
                connectionStart()
            }, 1000);
              }
              */
    }
    connection.onerror = function (error) {
        // Debug
        console.log('WebSocket Error ' + error);
        if (connection.readyState !== WebSocket.CLOSED) {
            connection.close();
        }
    }
    connection.onmessage = function (e) {
        // Debug
        console.log('WebSocket incomming message: ' + e.data);
        RefershData(e.data)
    }

    function KeepAlive() {
        var timeout = 1000;
        if (connection.readyState == WebSocket.OPEN) {
            connection.send("KeepAlive");
        }
        setTimeout(KeepAlive, timeout);
    }
}

function RefershData(input) {
    // Prüfen ob es ein Json ist
    if (!input.startsWith("{")) {
        return;
    }

    var json = $.parseJSON(input);
    // Log Json
    if (json.log) {
        var logArea = $('#log');
        logArea.append("[" + json.log.timeStamp + "] " + json.log.function + ": " + json.log.message + "\n");
        logArea.scrollTop(logArea[0].scrollHeight);

    } else {
        $.each(json, function (key, val) {
            // Config Json
            if (pageName == 'config') {
                if (typeof val === 'boolean') {
                    $("#" + key).prop('checked', val);
                } else {
                    $("#" + key).val(val.toString());
                }
            }
            // SystemInfo Json
            if (pageName == 'dash') {
                switch (key) {
                    case "pixelitVersion":
                        val = val != currentGitRelease ? `<i class="red">${val}<i>` : val;
                        break;
                    case "note":
                        if (!val.trim()) {
                            val = "---";
                        } else if (!titlechanged) {
                            document.title += " [" + val + "]";
                            titlechanged = true;
                        }
                        break;
                    case "sketchSize":
                    case "freeSketchSpace":
                    case "freeHeap":
                        val = humanFileSize(val, true);
                        break;
                    case "wifiRSSI":
                        val += " dBm";
                        break;
                    case "wifiQuality":
                        val += " %";
                        break;
                    case "cpuFreqMHz":
                        val += " MHz";
                        break;
                    case "sleepMode":
                        val = (val ? "On" : "Off");
                        break;
                }

                $("#" + key).html(val.toString());
            }
        });
    }
}

function SaveConfig() {
    var obj = {};
    // Alle Inputs auslesen
    $("input").each(function () {
        // Debug
        console.log('SaveConfig -> ID: ' + this.id + ', Val: ' + (this.type == 'checkbox' ? $(this)
            .prop('checked') : $(this).val()));

        if (this.type == 'checkbox') {
            obj[this.id] = $(this).prop('checked');
        } else {
            obj[this.id] = $(this).val();
        }
    });

    // Alle Selects auslesen
    $("select").each(function () {
        // Debug
        console.log('SaveConfig -> ID: ' + this.id + ', Val: ' + $(this).val());
        obj[this.id] = $(this).val();
    });

    json = JSON.stringify(obj);
    // Debug
    console.log(json);

    pageName = "setConfig";

    connectionStart();

    // Restart Countdown usw.
    var timeout = 12000;
    StartCountDown(timeout / 1000);

    setTimeout(function () {
        $("#popup").modal('hide');
    }, timeout);

    setTimeout(function () {
        $("#dash").click();
    }, timeout + 500);
}

function ChangePage(_pageName) {
    pageName = _pageName;

    // Dev Option
    if (devMode) {
        $("#mainContent").load("./" + _pageName + '.html')
    } else {
        $("#mainContent").load("./" + _pageName)
    }

    switch (_pageName) {
        case "testarea":
            pageName = 'setScreen';
            break;
        case "dash":
            currentGitRelease = getCurrentGitRelease();
            console.log(`currentGitRelease: ${currentGitRelease}`);
            break;
    }
}

// Countdown
function StartCountDown(_timeleft) {
    timeleft = _timeleft;
    rebootTimer = setInterval(function () {
        timeleft--;
        $("#countdowntimer").html(timeleft);
        if (timeleft <= 0)
            clearInterval(rebootTimer);
    }, 1000);
}

function SendTest(type, input) {

    if (isNullOrWhitespace(input)) {
        return;
    }

    var obj = {};

    switch (type) {
        case 'brightness':
            obj["brightness"] = input;
            obj = JSON.stringify(obj);
            break;
        case 'text':
            obj["text"] = {};
            obj["text"]["textString"] = input;
            obj["text"]["scrollText"] = "auto";
            obj["text"]["scrollTextDelay"] = 45;
            obj["text"]["bigFont"] = false;
            obj["text"]["centerText"] = false;
            obj["text"]["position"] = {};
            obj["text"]["position"]["x"] = 0;
            obj["text"]["position"]["y"] = 1;
            obj["text"]["color"] = {};
            obj["text"]["color"]["r"] = 255;
            obj["text"]["color"]["g"] = 255;
            obj["text"]["color"]["b"] = 255;
            obj = JSON.stringify(obj);
            break;
        case 'json':
            obj = input;
            break;
        case 'bitmap':
            if (input.includes('],[')) {
                if (!input.startsWith('[[')) {
                    input = '[' + input + ']';
                }
                obj["bitmapAnimation"] = {};
                obj["bitmapAnimation"]["data"] = input;
                obj["bitmapAnimation"]["animationDelay"] = 200;
            } else {
                obj["bitmap"] = {};
                obj["bitmap"]["data"] = input;
                obj["bitmap"]["position"] = {};
                obj["bitmap"]["position"]["x"] = 0;
                obj["bitmap"]["position"]["y"] = 0;
                obj["bitmap"]["size"] = {};
                obj["bitmap"]["size"]["height"] = 8;
                if (input.split(",").length == 64) {
                    obj["bitmap"]["size"]["width"] = 8;
                } else {
                    obj["bitmap"]["size"]["width"] = 32;
                }
            }
            obj = JSON.stringify(obj);
            obj = obj.replace("\"[", "[");
            obj = obj.replace("]\"", "]");
            break;
    }
    // Debug
    console.log(obj);
    connection.send(obj);
}

function isNullOrWhitespace(input) {

    if (typeof input === 'undefined' || input == null) {
        return true;
    }
    return input.replace(/\s/g, '').length < 1;
}

function humanFileSize(bytes, si = false, dp = 1) {
    const thresh = si ? 1000 : 1024;

    if (Math.abs(bytes) < thresh) {
        return bytes + ' B';
    }

    const units = si
        ? ['kB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB']
        : ['KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB'];
    let u = -1;
    const r = 10 ** dp;

    do {
        bytes /= thresh;
        ++u;
    } while (Math.round(Math.abs(bytes) * r) / r >= thresh && u < units.length - 1);


    return bytes.toFixed(dp) + ' ' + units[u];
}

function getCurrentGitRelease(){
    fetch('https://api.github.com/repos/o0shojo0o/PixelIt/releases')
    .then(res => res.json())
    .then(data => { return data[0].tag_name }) 
    .catch(error => console.error('Error:', error));    
}

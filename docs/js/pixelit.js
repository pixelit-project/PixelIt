//@ts-check
const ipAddress = $(location).attr('hostname');
let pageName = 'dash';
let devMode = false;
let timeleft;
let rebootTimer;
let json;
let titlechanged = false;
let gitData;

if (ipAddress.includes('localhost')) {
    devMode = true;
}

$(function () {
     // Akive Menu Button select 
     $('.nav-link').click(function () {
        $('.nav-link').removeClass('active');
        $(this).addClass('active');
    });
    ChangePage("dash");
});

let connection = null;

function capitalizeFirstLetter(string) {
    return string.charAt(0).toUpperCase() + string.slice(1);
}

function connectionStart() {
    if (connection != null && connection.readyState != WebSocket.CLOSED) {
        connection.close();
    }

    let wsServer = ipAddress;
    // Dev Option
    if (devMode) {
        wsServer = '192.168.0.137';
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
    }
    connection.onclose = function (e) {
        // Debug
        console.log('WebSocket connection close');
        $("#connectionStatus").html("Offline");
        $("#connectionStatus").removeClass("text-success");
        $("#connectionStatus").addClass("text-danger");
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
}

function RefershData(input) {
    // Prüfen ob es ein Json ist
    if (!input.startsWith("{")) {
        return;
    }

    const jsonObj = $.parseJSON(input);
    // Log Json
    if (jsonObj.log) {
        const logArea = $('#log');
        logArea.append(`[${jsonObj.log.timeStamp}] ${jsonObj.log.function}: ${jsonObj.log.message}\n`);
        logArea.scrollTop(logArea[0].scrollHeight);
    } else {
        for (const key in jsonObj) {
            value = jsonObj[key];
            // Config Json
            if (pageName == 'config') {
                if (typeof value === 'boolean') {
                    $("#" + key).prop('checked', value);
                } else {
                    $("#" + key).val(value.toString());
                }
            }
            // SystemInfo Json
            if (pageName == 'dash') {
                switch (key) {
                    case "pixelitVersion":                                                
                            value = checkUpdateavailable(value) ? `<a href="#" class="update" onclick="showChangelog();">${value} update available!</a>  <a href="#" onclick="showChangelog();"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="feather feather-info"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg></a>` : `<a href="#" onclick="showChangelog();">${value}</a>`;                                       
                        break;
                    case "note":
                        if (!value.trim()) {
                            value = "---";
                        }
                        break;
                    case "hostname":
                        if (value.trim() && !titlechanged) {
                            document.title += `[${value}]`;
                            titlechanged = true;
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
                        value = (value ? "On" : "Off");
                        break;
                    case "temperature":
                        if (typeof value == "number") {
                            value = (value).toFixed(1) + " °C"
                        }
                        break;
                    case "humidity":
                        if (typeof value == "number") {
                            value = (value).toFixed(1) + " %"
                        }
                        break;
                    case "pressure":
                        if (typeof value == "number") {
                            value = Math.round(value) + " hPa"
                        }
                        break;
                }

                $("#" + key).html(value.toString());
            }
        }
    }
}

function SaveConfig() {
    const obj = {};
    // Alle Inputs auslesen
    $("input").each(function () {
        // Debug
        console.log(`SaveConfig -> ID: ${this.id}, Val: ${(this.type == 'checkbox' ? $(this).prop('checked') : $(this).val())}`);
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
    const timeout = 12000;
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

    let obj = {};

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

function checkUpdateavailable(curentVersion) {    
    if (gitData && gitData[0] && gitData[0].name){
        return curentVersion != gitData[0].name;
    }
    getCurrentGitReleaseData();
    return false;
}

function showChangelog(){
    $('#changelog_modal_title').html(`Changelog for version ${gitData[0].name}`);
    $('#changelog_modal_body').html(`<ul>${gitData[0].body.replaceAll('-','<li>').replaceAll('\r\n','</li>')}</li></ul>`);
    $('#changelog_modal_button').attr("href", gitData[0].html_url)
    $('#changelog_modal').modal('show')           
}

function createDownloadStats(){
    let html = '';
    for(const key in gitData) {
        const assets = gitData[key].assets;
        const totalDownloads = assets.reduce((result, x) => result + x.download_count, 0);
        const readmeLink = `https://github.com/o0shojo0o/PixelIt#${gitData[key].name.replaceAll('.','')}-${gitData[key].published_at.split('T')[0]}`;
        html+= `<li><a href='${readmeLink}' target='_blank'><b>Version ${gitData[key].name}</b> - ${gitData[key].published_at.split('T')[0]}</a> - [${totalDownloads}]</li>`;
        html+= `<ul>`;
        for(const kkey in assets){        
            html+= `<li>${assets[kkey].name.replace('firmware_','').replace('.bin','').toLowerCase()} - [${assets[kkey].download_count}]</li>`;
        }
        html+= `</ul>`;   
        html+= `<br />`;           
    }

    $("#downloadStats").html(html);
}

async function getCurrentGitReleaseData() {
    try {
        gitData = (await(await fetch('https://api.github.com/repos/o0shojo0o/PixelIt/releases')).json());       
        console.log('getCurrentGitReleaseData: successfull');
    } catch (error) {
        console.log(`getCurrentGitReleaseData: error (${error})`);  
    }
}   

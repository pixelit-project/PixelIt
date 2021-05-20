const char mainPage[] PROGMEM = R"=====(
    <!doctype html>
    <html lang="en">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
        <link rel="stylesheet" href="https://o0shojo0o.github.io/PixelIt/css/bootstrap.min.css">
        <link rel="stylesheet" href="https://o0shojo0o.github.io/PixelIt/css/dashboard.css">
        <script src="https://o0shojo0o.github.io/PixelIt/js/jquery-3.3.1.min.js"> </script>
        <script src="https://o0shojo0o.github.io/PixelIt/js/pixelit.js"> </script>
        <title>PixelIt the Matrix Display</title>
    </head>
    <body>
        <nav class="navbar navbar-dark sticky-top bg-dark flex-md-nowrap p-0">
            <a class="navbar-brand col-sm-3 col-md-2 mr-0" href="#"><b>PixelIt</b> the Matrix Display</a>
            <ul class="navbar-nav px-3">
                <li class="nav-item text-nowrap">
                    <div class="text-center nav-link">
                        Connection: <span id="connectionStatus" class="text-danger">Offline</span>
                    </div>
                </li>
            </ul>
        </nav>
        <div class="container-fluid">
            <div class="row">            
                <nav class="col-md-2 d-none d-md-block bg-light sidebar">                 
                    <ul class="nav flex-column">
                        <li class="nav-item">
                            <a class="nav-link active" id="dash" onclick='ChangePage("dash")' href="#">
                                <span data-feather="cpu"></span> Dashboard
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" onclick='ChangePage("config")' href="#">
                                <span data-feather="sliders"></span> Options
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" onclick='ChangePage("testarea")' href="#">
                                <span data-feather="codepen"></span> Test Area
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" onclick='ChangePage("fwfsupdate")' href="#">
                                <span data-feather="upload"></span> Update
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" href="https://pixelit.bastelbunker.de/PixelGallery" target="blank">
                                <span data-feather="image"></span> Pixel Gallery
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" href="https://pixelit.bastelbunker.de/PixelCreator" target="blank">
                                <span data-feather="edit"></span> Pixel Creator
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" href="https://forum.bastelbunker.de/t/pixel-it" target="blank">
                                <span data-feather="layers"></span> Forum
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" href="https://www.bastelbunker.de/pixel-it/" target="blank">
                                <span data-feather="pen-tool"></span> Blog
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" href="https://docs.bastelbunker.de/pixelit/" target="blank">
                                <span data-feather="book-open"></span> Documentation
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" href="https://github.com/o0shojo0o/PixelIt" target="blank">
                                <span data-feather="github"></span> GitHub
                            </a>
                        </li>
                    </ul>     
                </nav>           
                <div class="offset-md-2 col-md-10 row" id="mainContent">
                    <div class="row col-md-12" id="mainRefresh">
    
                    </div>
                </div>
            </div>
            <script src="https://o0shojo0o.github.io/PixelIt/js/bootstrap.min.js">
            </script>
            <script src="https://o0shojo0o.github.io/PixelIt/js/feather.min.js"></script>
            <script>
                feather.replace()
            </script>
        </body>
    </html>
)=====";

const char configPage[] PROGMEM = R"=====(
<script>
    connectionStart();
</script>
<div class="col-md-2">
    <h2 class="text-center">Matrix</h2>
    <hr>
    <div class="form-group">
        <label for="matrixType">Matrix Type</label>
        <select type="select" class="form-control" id="matrixType">
            <option value="1">Type 1</option>
            <option value="2">Type 2</option>
            <option value="3">Type 3</option>
        </select>
    </div>
    <div class="form-group">
        <label for="matrixTempCorrection">Matrix Correction</label>
        <select type="select" class="form-control" id="matrixTempCorrection">
            <option value="default">Default</option>
            <option value="typicalsmd5050">TypicalSMD5050</option>
            <option value="typical8mmpixel">Typical8mmPixel</option>
            <option value="tungsten40w">Tungsten40W</option>
            <option value="tungsten100w">Tungsten100W</option>
            <option value="halogen">Halogen</option>
            <option value="carbonarc">CarbonArc</option>
            <option value="highnoonsun">HighNoonSun</option>
            <option value="directsunlight">DirectSunlight</option>
            <option value="overcastsky">OvercastSky</option>
            <option value="clearbluesky">ClearBlueSky</option>
            <option value="warmfluorescent">WarmFluorescent</option>
            <option value="standardfluorescent">StandardFluorescent</option>
            <option value="coolwhitefluorescent">CoolWhiteFluorescent</option>
            <option value="fullspectrumfluorescent">FullSpectrumFluorescent</option>
            <option value="growlightfluorescent">GrowLightFluorescent</option>
            <option value="blacklightfluorescent">BlackLightFluorescent</option>
            <option value="mercuryvapor">MercuryVapor</option>
            <option value="sodiumvapor">SodiumVapor</option>
            <option value="metalhalide">MetalHalide</option>
            <option value="highpressuresodium">HighPressureSodium</option>
        </select>
    </div>
    <div class="form-group">
        <label for="hostname">Hostname</label>
        <input type="text" class="form-control" id="hostname">
    </div>
    <div class="form-group">
        <label for="note">Note</label>
        <input type="text" class="form-control" id="note">
    </div>
    <div class="custom-control custom-switch">
        <input type="checkbox" class="custom-control-input" id="bootScreenAktiv">
        <label class="custom-control-label" for="bootScreenAktiv">Bootsceen active</label>
    </div>
    <br>
    <h2 class="text-center">Auto brightness</h2>
    <hr>
    <div class="form-inline">              
        <label for="mbaDimMin" class="col-md-5">Min bright</label> 
        <label for="mbaDimMax" class="col-md-6 offset-md-1">Max bright</label>
    </div>
    <div class="form-inline"> 
        <input type="number" class="form-control col-md-5 text-center" id="mbaDimMin" value="20" step="1" min="0">            
        <input type="number" class="form-control col-md-5 offset-md-2 text-center" id="mbaDimMax" value="255" step="1" min="0" max="255">
    </div>
    <div class="form-inline">              
        <label for="mbaLuxMin" class="col-md-5">From lux</label> 
        <label for="mbaLuxMax" class="col-md-5 offset-md-2">To lux</label>
    </div>
    <div class="form-inline"> 
        <input type="number" class="form-control col-md-5 text-center" id="mbaLuxMin" value="0" step="1" min="0">            
        <input type="number" class="form-control col-md-5 offset-md-2 text-center" id="mbaLuxMax" value="400" step="1" min="0">
    </div> 
    <br>
    <div class="custom-control custom-switch">
        <input type="checkbox" class="custom-control-input" id="matrixBrightnessAutomatic">
        <label class="custom-control-label" for="matrixBrightnessAutomatic">Auto brightness active</label>
    </div>
</div>

<div class="offset-md-1 col-md-2">
    <h2 class="text-center">Time</h2>
    <hr>
    <div class="form-group">
        <label for="ntpServer">NTP Server</label>
        <input type="text" class="form-control" id="ntpServer">
    </div>
    <div class="form-group">
        <label for="clockTimeZone">UTC Offset</label>
        <input type="number" class="form-control" id="clockTimeZone">
    </div>
    <div class="form-group">
        <label for="clockColor">Color</label>
        <input type="color" class="form-control" value="#ff0000" id="clockColor">
    </div>
     <div class="custom-control custom-switch">
        <input type="checkbox" class="custom-control-input" id="clockWithSeconds">
        <label class="custom-control-label" for="clockWithSeconds">Clock with sek</label>
    </div>
    <div class="custom-control custom-switch">
        <input type="checkbox" class="custom-control-input" id="clockSwitchAktiv">
        <label class="custom-control-label" for="clockSwitchAktiv">Switch clock/date active</label>
    </div>
    <div class="form-group">Switch clock/date time</label>
        <input type="number" class="form-control" id="clockSwitchSec">
    </div>
</div>

<div class="offset-md-1 col-md-2">
    <h2 class="text-center">Defaults</h2>
    <hr>
    <div class="form-group">
        <label for="matrixtBrightness">Matrix Brightness</label>
        <input type="number" class="form-control" id="matrixBrightness">
    </div>
    <div class="form-group">
        <label for="scrollTextDefaultDelay">ScrollText Delay</label>
        <input type="number" class="form-control" id="scrollTextDefaultDelay">
    </div>
</div>

<div class="offset-md-1 col-md-2">
    <h2 class="text-center">MQTT</h2>
    <hr>
    <div class="form-group">
        <label for="mqttServer">Server</label>
        <input type="text" class="form-control" id="mqttServer" placeholder="IP or Hostname">
    </div>
    <div class="form-group">
        <label for="mqttPort">Port</label>
        <input type="number" class="form-control" id="mqttPort">
    </div>
    <div class="form-group">
        <label for="mqttUser">User</label>
        <input type="text" class="form-control" id="mqttUser" placeholder="Optional">
    </div>
    <div class="form-group">
        <label for="mqttPassword">Password</label>
        <input type="password" class="form-control" id="mqttPassword" placeholder="Optional">
    </div>
    <div class="form-group">
        <label for="mqttMasterTopic">MasterTopic</label>
        <input type="text" class="form-control" id="mqttMasterTopic">
    </div>
    <div class="custom-control custom-switch">
        <input type="checkbox" class="custom-control-input" id="mqttAktiv">
        <label class="custom-control-label" for="mqttAktiv">MQTT active</label>
    </div>
</div>
<div class="col-md-12 text-center">
    <hr />
</div>
<div class="col-md-12 text-center">
    <button type="button" class="btn btn-outline-success" onclick="SaveConfig()" data-toggle="modal"
        data-backdrop="static" data-keyboard="false" data-target="#popup"><span data-feather="save"></span> Save
        Config</button>
</div>

<div class="modal fade" id="popup">
    <div class="modal-dialog modal-dialog-centered modal-lg">
        <div class="modal-content">
            <div class="modal-header bg-light">
                <h3 class="modal-title text-success">Config saved!</h3>
            </div>
            <div class="modal-body">
                <h5>PixelIt will now be restarted, please wait <span id="countdowntimer">12 </span> seconds to reload!
                </h5>
            </div>
        </div>
    </div>
</div>

<script>
    feather.replace()
</script>
)=====";

const char dashPage[] PROGMEM = R"=====(
<script>
    connectionStart();
</script>
<div class="col-md-3">
    <h2 class="text-center">PixelIt System Info</h2>
    <hr>
    <div class="table-responsive">
        <table class="table table-striped table-sm" id="table">
            <tbody>
                <tr>
                    <td class="font-weight-bold">PixelIt Version:</td>
                    <td id="pixelitVersion">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Hostname:</td>
                    <td id="hostname">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Note:</td>
                    <td id="note">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Sketch Size:</td>
                    <td id="sketchSize">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Free Sketch Space:</td>
                    <td id="freeSketchSpace">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Wifi RSSI:</td>
                    <td id="wifiRSSI">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Wifi Quality:</td>
                    <td id="wifiQuality">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Wifi SSID:</td>
                    <td id="wifiSSID">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">IP-Address:</td>
                    <td id="ipAddress">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Free Heap:</td>
                    <td id="freeHeap">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">ChipID:</td>
                    <td id="chipID">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">CPU Freq.:</td>
                    <td id="cpuFreqMHz">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Sleep Mode:</td>
                    <td id="sleepMode">Loading...</td>
                </tr>
            </tbody>
        </table>
    </div>
</div>
<div class="col-md-3">
    <h2 class="text-center">Sensors</h2>
    <hr>
    <div class="table-responsive">
        <table class="table table-striped table-sm" id="table">
            <tbody>
                <tr>
                    <td class="font-weight-bold">Lux Sensor:</td>
                    <td id="lux">Loading...</td>
                </tr>
                <tr>
                    <td class="font-weight-bold">Temperature Sensor:</td>
                    <td id="temperature">Loading...</td>
                </tr>
				<tr>
                    <td class="font-weight-bold">Humidity Sensor:</td>
                    <td id="humidity">Loading...</td>
                </tr>
            </tbody>
        </table>
    </div>
</div>
<div class="col-md-6">
    <h2 class="text-center">Log</h2>
    <hr>
    <textarea class="form-control" id="log" rows="17" wrap="off" disabled style="font: 60"></textarea>
</div>
<div class="modal" tabindex="-1" role="dialog" id="changelog_modal">
    <div class="modal-dialog" role="document">
      <div class="modal-content">
        <div class="modal-header bg-light">
          <h5 class="modal-title" id="changelog_modal_title"></h5>
          <button type="button" class="close" data-dismiss="modal" aria-label="Close">
            <span aria-hidden="true">&times;</span>
          </button>
        </div>
        <div class="modal-body" id="changelog_modal_body">    
        </div>
        <div class="modal-footer">
            <a href="" target="_blank" id="changelog_modal_button" type="button" class="btn btn-primary">Got to download</a>
          </div>
      </div>
    </div>
  </div>
)=====";

const char testAreaPage[] PROGMEM = R"=====(
<script>
    connectionStart();
    $("#brightness").change(function () {
        var val = $("#brightness").val();
        $("#brightnessVal").html(val);
        SendTest('brightness', val);
    });
</script>
<div class="col-md-4">
    <h2 class="text-center">JSON</h2>
    <hr>
    <textarea class="form-control" id="json" rows="17" wrap="off"
        placeholder='{"text":{"textString":"Test It :D","scrollText":"auto","bigFont":false,"centerText":false,"position":{"x":0,"y":1},"color":{"r":255,"g":255,"b":255}}}}'></textarea>
    <br />
    <div class="col-md-12 text-center">
        <button type="button" class="btn btn-outline-primary" onclick="SendTest('json',$('#json').val())"><span
                data-feather="send"></span>
            Send
            JSON</button>
    </div>
</div>
<div class="col-md-4">
    <h2 class="text-center">Text / Brightness</h2>
    <hr>
    <label for="brightness">Brightness:</label>
    <input type="range" class="custom-range" min="0" max="255" id="brightness">
    <div class="text-center" id="brightnessVal">127</div>
    <br />
    <div class="form-group">
        <label for="text">Text:</label>
        <input type="text" class="form-control" id="text" placeholder="Enter yor text...">
    </div>
    <br />
    <div class="col-md-12 text-center">
        <button type="button" class="btn btn-outline-primary" onclick="SendTest('text',$('#text').val())"><span
                data-feather="send"></span>
            Send
            Text</button>
    </div>
</div>
<div class="col-md-4">
    <h2 class="text-center">Bitmap</h2>
    <hr>
    <textarea class="form-control" id="bitmap" rows="17" wrap="off"
        placeholder='[ 0,0,0......] or [[ 0,0,0......],[ 0,0,0......],[ 0,0,0......],[ 0,0,0......]]'></textarea>
    <br />
    <div class="col-md-12 text-center">
        <button type="button" class="btn btn-outline-primary" onclick="SendTest('bitmap',$('#bitmap').val())"><span
                data-feather="send"></span>
            Send
            Bitmap</button>
    </div>
</div>

<script>
    feather.replace()
</script>
)=====";

const char updatePage[] PROGMEM = R"=====(
<div class="col-md-6">
    <h2 class="text-center">Firmware Update</h2>
    <hr>
    <form method='POST' action='/update' enctype='multipart/form-data' class="text-center">
        <input type='file' accept='.bin,.bin.gz' name='firmware' id="firmware">
        <br />
        <br />
        <div class="col-md-12">
            <button type="submit" class="btn btn-outline-primary"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="feather feather-send"><line x1="22" y1="2" x2="11" y2="13"></line><polygon points="22 2 15 22 11 13 2 9 22 2"></polygon></svg> Update Firmware</button>
        </div>
    </form>
</div>
<div class="col-md-6">
<h2 class="text-center">Filesystem Update</h2>
    <hr>
    <form method='POST' action='/update' enctype='multipart/form-data' class="text-center">
        <input type='file' accept='.bin,.bin.gz' name='filesystem'>
        <br />
        <br />
        <div class="col-md-12 text-center">
            <button type="submit" class="btn btn-outline-primary"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="feather feather-send"><line x1="22" y1="2" x2="11" y2="13"></line><polygon points="22 2 15 22 11 13 2 9 22 2"></polygon></svg> Update Filesystem</button>
        </div>
    </form>
</div>
)=====";

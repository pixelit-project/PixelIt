import mmap
import os
import re
Import("env")

SOURCECODE = "./include/Firmware.h"

# Access to global construction environment
build_tag = env['PIOENV']


# Dump construction environment (for debug purpose)
# print(env.Dump())

# Search Version
version = ""
file1 = open(SOURCECODE, 'r')
Lines = file1.readlines()

count = 0
# Strips the newline character
for line in Lines:
    match = re.search(r"^#define\s+VERSION\s+[\"'](.*)[\"'].*$", line)
    if match:
        version = match.group(1)
        file1.close
        break

if version == "":
    raise Exception(f"No version found in {SOURCECODE}")

# Rename binary according to environnement/board
# ex: firmware_esp32dev.bin or firmware_nodemcuv2.bin
env.Replace(PROGNAME=f"firmware_v{version}_{build_tag}")

# Avoid a second full HTTP POST body on the ESP8266's small heap. Build against
# a local header copy, leaving the installed pinned framework untouched.
if env.PioPlatform().name == "espressif8266":
    from pathlib import Path
    import shutil
    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif8266"))
    original = framework / "libraries" / "ESP8266WebServer" / "src"
    patched = Path(env.subst("$BUILD_DIR")) / "pixelit_http"
    shutil.copytree(original, patched, dirs_exist_ok=True)
    shutil.copy2(Path("include/PixelItHttpBody.h"), patched / "PixelItHttpBody.h")
    parser = patched / "Parsing-impl.h"
    contents = parser.read_text()
    old = "arg.value = plainBuf;"
    if contents.count(old) != 1:
        raise RuntimeError("Review ESP8266 HTTP body move patch for this framework version")
    contents = contents.replace(old, "arg.value = std::move(plainBuf);")
    # Stream/compact only the screen JSON body. Other endpoints, multipart OTA
    # and URL-encoded forms retain the pinned core's parser and move patch.
    contents = '#include "PixelItHttpBody.h"\n' + contents
    screen_reader = """    String plainBuf;
    if (   !isForm"""
    screen_replacement = """    String plainBuf;
    if (!isForm && !isEncoded && method == HTTP_POST && url == F("/api/screen")) {
      const int status = ReadScreenHttpBody(client, contentLength, plainBuf, HTTP_MAX_POST_WAIT);
      if (status != 200) {
        _keepAlive = false;
        sendHeader(F("Connection"), F("close"));
        sendHeader(F("Access-Control-Allow-Origin"), "*");
        send(status, "application/json", ScreenHttpError(status));
        return CLIENT_REQUEST_IS_HANDLED;
      }
    } else if (   !isForm"""
    if contents.count(screen_reader) != 1:
        raise RuntimeError("Review ESP8266 screen body reader patch for this framework version")
    contents = contents.replace(screen_reader, screen_replacement)
    # Core 3 reads through S2Stream, otherwise growing/reallocating the String
    # repeatedly while TCP data arrives. Reserve the advertised size once.
    reader = "  S2Stream dataStream(data);"
    if contents.count(reader) != 1:
        raise RuntimeError("Review ESP8266 HTTP body reserve patch for this framework version")
    contents = contents.replace(reader, "  if (!data.reserve(maxLength + 1)) return false;\n" + reader)
    parser.write_text(contents)
    env.Prepend(CPPPATH=[str(patched)])
    # Keep the same template implementation first in library builds as well.
    env.Prepend(CCFLAGS=["-I" + str(patched)])

    import sys
    sys.path.insert(0, str(Path(".").resolve()))
    from transport_patches import patch_mqtt, patch_websocket
    transport_dir = Path(env.subst("$BUILD_DIR")) / "pixelit_transport"
    transport_dir.mkdir(parents=True, exist_ok=True)
    for header in ("PixelItTransport.h", "ScreenJsonCompactor.h", "ScreenLimits.h"):
        shutil.copy2(Path("include") / header, transport_dir / header)

    def transport_source(build_env, node):
        source = Path(node.srcnode().get_abspath())
        patch = {"PubSubClient.cpp": patch_mqtt, "WebSockets.cpp": patch_websocket}.get(source.name)
        if patch is None:
            return node
        target = transport_dir / source.name
        text = patch(source.read_text())
        if not target.exists() or target.read_text() != text:
            target.write_text(text)
        compile_env = build_env.Clone()
        compile_env.Prepend(CPPPATH=[str(source.parent), str(transport_dir)])
        # ESP8266's linker maps *.cpp.o into flash; preserve that suffix.
        return compile_env.Object(str(target) + ".o", str(target))[0]

    env.AddBuildMiddleware(transport_source, "*PubSubClient.cpp")
    env.AddBuildMiddleware(transport_source, "*WebSockets.cpp")

# The ESP32 pinned WebServer is compiled from .cpp, rather than templates.
# Only screen JSON reception changes; URL forms and multipart OTA stay upstream.
if env.PioPlatform().name == "espressif32":
    from pathlib import Path
    import sys
    sys.path.insert(0, str(Path(".").resolve()))
    from transport_patches import once, patch_mqtt, patch_websocket
    transport_dir = Path(env.subst("$BUILD_DIR")) / "pixelit_transport"
    transport_dir.mkdir(parents=True, exist_ok=True)

    def esp32_source(build_env, node):
        source = Path(node.srcnode().get_abspath())
        contents = source.read_text()
        if source.name == "Parsing.cpp":
            contents = '#include "PixelItHttpBody.h"\n' + contents
            contents = once(contents, '    if (!isForm){', '''    if (!isForm && !isEncoded && method == HTTP_POST && url == F("/api/screen")) {
      String body;
      const int status = ReadScreenHttpBody(client, _clientContentLength, body, HTTP_MAX_POST_WAIT);
      if (status != 200) {
        sendHeader(F("Connection"), F("close"));
        sendHeader(F("Access-Control-Allow-Origin"), "*");
        send(status, "application/json", ScreenHttpError(status));
        return false;
      }
      _parseArguments(searchStr);
      RequestArgument& arg = _currentArgs[_currentArgCount++];
      arg.key = F("plain");
      arg.value = std::move(body);
    } else if (!isForm){''')
        elif source.name == "WebSocketsServer.cpp":
            contents = '#include <Arduino.h>\n#include <fcntl.h>\n' + contents
            contents = once(contents, 'client->tcp->setNoDelay(true);',
                            'client->tcp->setNoDelay(true);\n            fcntl(client->tcp->fd(), F_SETFL, O_NONBLOCK);')
        elif source.name == "PubSubClient.cpp":
            contents = patch_mqtt(contents, esp32=True)
        elif source.name == "WebSockets.cpp":
            contents = patch_websocket(contents, esp32=True)
        else:
            return node
        target = transport_dir / source.name
        if not target.exists() or target.read_text() != contents:
            target.write_text(contents)
        compile_env = build_env.Clone()
        compile_env.Prepend(CPPPATH=[str(source.parent), str(Path("include").resolve())])
        return compile_env.Object(str(target) + ".o", str(target))[0]

    env.AddBuildMiddleware(esp32_source, "*WebServer/src/Parsing.cpp")
    env.AddBuildMiddleware(esp32_source, "*PubSubClient.cpp")
    env.AddBuildMiddleware(esp32_source, "*WebSockets.cpp")

    env.AddBuildMiddleware(esp32_source, "*WebSocketsServer.cpp")


# Instrument local copies of the pinned OTA handlers; preserve their routes,
# authentication, flash/filesystem handling and responses.
from pathlib import Path
import shutil
framework_name = ("framework-arduinoespressif8266" if env.PioPlatform().name == "espressif8266"
                  else "framework-arduinoespressif32")
updater_name = "ESP8266HTTPUpdateServer" if env.PioPlatform().name == "espressif8266" else "HTTPUpdateServer"
updater_source = Path(env.PioPlatform().get_package_dir(framework_name)) / "libraries" / updater_name / "src"
updater_target = Path(env.subst("$BUILD_DIR")) / "pixelit_updater"
shutil.copytree(updater_source, updater_target, dirs_exist_ok=True)
implementation = updater_target / (updater_name + ("-impl.h" if updater_name.startswith("ESP8266") else ".h"))
contents = implementation.read_text()
anchor = "      esp_yield();" if updater_name.startswith("ESP8266") else "                delay(0);"
if contents.count(anchor) != 1:
    raise RuntimeError("Review OTA display hook for this framework version")
length = "upload.contentLength" if updater_name.startswith("ESP8266") else '_server->header("Content-Length").toInt()'
hook = """      if (_authenticated) {
        FirmwareUpdateUpload(upload.status, upload.totalSize +
            (upload.status == UPLOAD_FILE_WRITE ? upload.currentSize : 0),
            %s, Update.hasError() || _updaterError.length());
      }
""" % length
contents = '#include "FirmwareUpdateDisplay.h"\n' + contents.replace(anchor, hook + anchor)
restart = "ESP.restart();"
if contents.count(restart) != 1:
    raise RuntimeError("Review OTA reboot display hook for this framework version")
contents = contents.replace(restart, "FirmwareUpdateReboot();\n        " + restart)
implementation.write_text(contents)
# Explicit include avoids depending on framework library search order.
(updater_target / "PixelItHTTPUpdateServer.h").write_text('#include "' + updater_name + '.h"\n')
env.Prepend(CPPPATH=[str(updater_target.resolve())])

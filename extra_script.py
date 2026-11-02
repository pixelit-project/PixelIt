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
    for header in ("PixelItTransport.h", "ScreenJsonCompactor.h"):
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

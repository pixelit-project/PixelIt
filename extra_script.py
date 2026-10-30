import mmap
import os
import re
Import("env")

SOURCECODE = "./src/PixelIt.ino"

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
    parser = patched / "Parsing-impl.h"
    contents = parser.read_text()
    old = "arg.value = plainBuf;"
    if contents.count(old) != 1:
        raise RuntimeError("Review ESP8266 HTTP body move patch for this framework version")
    contents = contents.replace(old, "arg.value = std::move(plainBuf);")
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

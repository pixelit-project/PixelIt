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

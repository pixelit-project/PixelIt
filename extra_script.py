  
Import("env")

# Access to global construction environment
build_tag = env['PIOENV']

# Dump construction environment (for debug purpose)
# print(env.Dump())

# Rename binary according to environnement/board
# ex: firmware_esp32dev.bin or firmware_nodemcuv2.bin
env.Replace(PROGNAME="firmware_%s" % build_tag)
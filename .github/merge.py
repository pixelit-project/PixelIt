import os
import shutil
import subprocess

root_dir = '.pio/build'
boot_app0_path = os.path.join(os.path.expanduser("~"),".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin")

print("+++ Starting merging... +++")

# Check if the boot_app0.bin file exists
if not os.path.isfile(boot_app0_path):
    print(f"boot_app0.bin not found at {boot_app0_path}")
    exit(1)

# Check if the root directory exists
if not os.path.isdir(root_dir):
    print(f"Root directory {root_dir} not found")
    exit(1)

# Iterate over all items in the root directory
for item in os.listdir(root_dir):
    item_path = os.path.join(root_dir, item)
    
    # Check if the item is a directory and its name starts with "esp32"
    if os.path.isdir(item_path) and item.lower().startswith("esp32"):
        print(f"Found an 'esp32' directory: {item_path}")

        # Check if the directory contains a file which begins with "firmware"
        firmware_path = ""
        for file in os.listdir(item_path):
            if file.lower().startswith("firmware") and file.lower().endswith(".bin") and "full-upgrade" not in file.lower():
                firmware_path = os.path.join(item_path, file)
                directory_path, filename_with_extension = os.path.split(firmware_path)
                filename, file_extension = os.path.splitext(filename_with_extension)
                print(f"> Found a 'firmware' file: {firmware_path}")

                # build new filename
                firmware_combined_path = os.path.join(directory_path, filename + ".full-upgrade" + file_extension)

                # copy boot_app0.bin
                print(f"> Copying boot_app0.bin to {item_path}...")
                shutil.copy(boot_app0_path, item_path)

                # merge firmware
                print(f"> Merging firmware to {firmware_combined_path}...")
                command = f"python -m esptool --chip esp32 merge_bin -o {firmware_combined_path} --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 {item_path}/bootloader.bin 0x8000 {item_path}/partitions.bin 0xe000 {item_path}/boot_app0.bin 0x10000 {firmware_path}"
                return_code  = subprocess.call(command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if return_code != 0:
                    print(f"> Merging failed with return code {return_code}.")
                    break
                else:
                    print("> Merging successful")

                print("> Done")
                break
        
        if firmware_path == "":
            print("> No firmware file for merging found")

print("+++ Merging done... +++")
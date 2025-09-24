# Source https://github.com/letscontrolit/ESPEasy/pull/3845#issuecomment-1005864664

# pylint: disable=E0602
Import("env")  # noqa

import os
import shutil

if os.environ.get("ESPHOME_USE_SUBPROCESS") is None:
    try:
        import esptool
    except ImportError:
        env.Execute("$PYTHONEXE -m pip install esptool")
else:
    import subprocess
from SCons.Script import ARGUMENTS

# Copy over the default sdkconfig.
from os import path

if path.exists("./sdkconfig.defaults"):
    os.makedirs(".temp", exist_ok=True)
    shutil.copy("./sdkconfig.defaults", "./.temp/sdkconfig-esp32-idf")


def esp32_create_combined_bin(source, target, env):
    verbose = bool(int(ARGUMENTS.get("PIOVERBOSE", "0")))
    if verbose:
        print("Generating combined binary for serial flashing")
    app_offset = 0x10000

    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    chip = env.get("BOARD_MCU")
    flash_size = env.BoardConfig().get("upload.flash_size")
    cmd = [
        "--chip",
        chip,
        "merge_bin",
        "-o",
        new_file_name,
        "--flash_size",
        flash_size,
    ]
    if verbose:
        print("    Offset | File")
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        if verbose:
            print(f" -  {sect_adr} | {sect_file}")
        cmd += [sect_adr, sect_file]

    cmd += [hex(app_offset), firmware_name]

    if verbose:
        print(f" - {hex(app_offset)} | {firmware_name}")
        print()
        print(f"Using esptool.py arguments: {' '.join(cmd)}")
        print()

    if os.environ.get("ESPHOME_USE_SUBPROCESS") is None:
        esptool.main(cmd)
    else:
        subprocess.run(["esptool.py", *cmd])


def esp32_copy_ota_bin(source, target, env):
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.ota.bin")

    shutil.copyfile(firmware_name, new_file_name)


# pylint: disable=E0602
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)  # noqa
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_copy_ota_bin)  # noqa

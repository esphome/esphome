# Source https://github.com/letscontrolit/ESPEasy/pull/3845#issuecomment-1005864664

import subprocess

# pylint: disable=E0602
Import("env")  # noqa


def esp32_create_combined_bin(source, target, env):
    print("Generating combined binary for serial flashing")
    app_offset = 0x10000

    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}-combined.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    chip = env.get("BOARD_MCU")
    cmd = [
        "esptool.py",
        "--chip",
        chip,
        "merge_bin",
    ]
    print("    Offset | File")
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        print(f" -  {sect_adr} | {sect_file}")
        cmd += [sect_adr, sect_file]

    print(f" - {hex(app_offset)} | {firmware_name}")
    cmd += [hex(app_offset), firmware_name]

    cmd += ["-o", new_file_name]

    subprocess.run(cmd, check=False)


# pylint: disable=E0602
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)  # noqa

# Source https://github.com/letscontrolit/ESPEasy/blob/bb8907ee87af8980f95141caf1415c37fa39c941/tools/pio/post_esp32.py

# pylint: disable=E0602
Import("env")  # noqa


def esp32_create_combined_bin(source, target, env):
    print("Generating combined binary for serial flashing")
    offset = 0x0
    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}-combined.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    with open(new_file_name, "wb") as new_file:
        for section in sections:
            sect_adr, sect_file = section.split(" ", 1)
            with open(sect_file, "rb") as extra:
                new_file.seek(int(sect_adr, 0) - offset)
                new_file.write(extra.read())

        with open(env.subst("$BUILD_DIR/${PROGNAME}.bin"), "rb") as firmware:
            new_file.seek(0x10000 - offset)
            new_file.write(firmware.read())


# pylint: disable=E0602
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)  # noqa

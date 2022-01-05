# Source https://github.com/letscontrolit/ESPEasy/blob/bb8907ee87af8980f95141caf1415c37fa39c941/tools/pio/post_esp32.py

Import("env")  # noqa


def esp32_create_combined_bin(source, target, env):
    print("Generating combined binary for serial flashing")
    offset = 0x0
    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}-combined.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    new_file = open(new_file_name, "wb")
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        source = open(sect_file, "rb")
        new_file.seek(int(sect_adr, 0) - offset)
        new_file.write(source.read())
        source.close()

    firmware = open(env.subst("$BUILD_DIR/${PROGNAME}.bin"), "rb")
    new_file.seek(0x10000 - offset)
    new_file.write(firmware.read())
    new_file.close()
    firmware.close()


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)  # noqa

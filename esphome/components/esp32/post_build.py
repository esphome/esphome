# Source https://github.com/letscontrolit/ESPEasy/pull/3845#issuecomment-1005864664

import os

# pylint: disable=E0602
Import("env")  # noqa


def decode_flash_size_speed(byte_value):
    speed = byte_value[0] & 0x0F
    size = int((byte_value[0] & 0xF0) / 16)

    # As defined in esp_image_spi_freq_t
    spi_speed = {0: "40", 1: "26", 2: "20", 0xF: "80"}
    speed_mhz = spi_speed.get(speed, "??")
    size_mb = str(1 << size)

    return f"0x{byte_value.hex()} ({size_mb}MB, {speed_mhz}MHz)"


def esp32_create_combined_bin(source, target, env):
    print("Generating combined binary for serial flashing")

    # offset = 0x1000
    offset = 0x0
    flash_page_size = 256

    # The offset from begin of the file where the app0 partition starts
    # This is defined in the partition .csv file
    sketch_partition_start = 0x10000

    # Typical layout of the generated file:
    #    Offset | File
    # -  0x1000 | ~\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32\bin\bootloader_dout_40m.bin
    # -  0x8000 | ~\ESPEasy\.pio\build\<env name>\partitions.bin
    # -  0xe000 | ~\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin
    # - 0x10000 | ~\ESPEasy\.pio\build\<env name>/<built binary>.bin

    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}-combined.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    with open(new_file_name, "wb") as new_file:
        new_file.truncate()  # Make sure no left over data is present from a previous build

        firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")

        # fill the file with 0xFF binary data to reflect 'erased and not yet written' data.
        # Make sure to use exactly the size that normally would be erased
        new_file.seek(0)
        total_size = sketch_partition_start + os.path.getsize(firmware_name)
        total_size = total_size - (total_size % flash_page_size) + flash_page_size

        new_file.write(bytes([0xFF] * total_size))

        print("    Offset | File")
        for section in sections:
            sect_adr, sect_file = section.split(" ", 1)
            print(f" -  {sect_adr} | {sect_file}")
            with open(sect_file, "rb") as source_file:
                new_file.seek(int(sect_adr, 0) - offset)
                new_file.write(source_file.read())
                if "bootloader" in sect_file:
                    # Need to patch the bootloader to match the flash size
                    flash_size = env.BoardConfig().get("upload.flash_size")

                    # See esp_image_header_t for struct definition
                    spi_speed_size_byte_offset = 3
                    source_file.seek(spi_speed_size_byte_offset)
                    spi_speed_size_byte = source_file.read(1)
                    new_spi_speed_size_byte = spi_speed_size_byte[0] & 0x0F

                    # As defined in esp_image_flash_size_t
                    spi_size = {
                        "1MB": 0x00,
                        "2MB": 0x10,
                        "4MB": 0x20,
                        "8MB": 0x30,
                        "16MB": 0x40,
                    }
                    new_spi_speed_size_byte |= spi_size.get(
                        flash_size, spi_speed_size_byte[0] & 0xF0
                    )

                    if new_spi_speed_size_byte != spi_speed_size_byte:
                        new_byte = bytes([new_spi_speed_size_byte])
                        print(
                            f"           | Patch flash size in bootloader: {decode_flash_size_speed(spi_speed_size_byte)} -> {decode_flash_size_speed(new_byte)}"
                        )
                        new_file.seek(
                            int(sect_adr, 0) - offset + spi_speed_size_byte_offset
                        )
                        new_file.write(new_byte)

        firmware_start = sketch_partition_start - offset
        with open(firmware_name, "rb") as firmware:
            print(f" - {hex(firmware_start)} | {firmware_name}")
            new_file.seek(firmware_start)
            new_file.write(firmware.read())


# pylint: disable=E0602
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)  # noqa

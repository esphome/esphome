import shutil

# pylint: disable=E0602
Import("env")  # noqa


def esp8266_copy_factory_bin(source, target, env):
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin")

    shutil.copyfile(firmware_name, new_file_name)


def esp8266_copy_ota_bin(source, target, env):
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.ota.bin")

    shutil.copyfile(firmware_name, new_file_name)


# pylint: disable=E0602
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp8266_copy_factory_bin)  # noqa
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp8266_copy_ota_bin)  # noqa

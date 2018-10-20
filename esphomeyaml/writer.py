from __future__ import print_function

import codecs
import errno
import json
import logging
import os
import shutil

from esphomeyaml import core
from esphomeyaml.config import iter_components
from esphomeyaml.const import ARDUINO_VERSION_ESP32_DEV, CONF_ARDUINO_VERSION, CONF_BOARD, \
    CONF_BOARD_FLASH_MODE, CONF_ESPHOMELIB_VERSION, CONF_ESPHOMEYAML, CONF_LOCAL, CONF_NAME, \
    CONF_USE_CUSTOM_CODE, ESP_PLATFORM_ESP32, CONF_REPOSITORY, CONF_COMMIT, CONF_BRANCH, CONF_TAG
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.core_config import VERSION_REGEX
from esphomeyaml.helpers import relative_path

_LOGGER = logging.getLogger(__name__)

CPP_AUTO_GENERATE_BEGIN = u'// ========== AUTO GENERATED CODE BEGIN ==========='
CPP_AUTO_GENERATE_END = u'// =========== AUTO GENERATED CODE END ============'
INI_AUTO_GENERATE_BEGIN = u'; ========== AUTO GENERATED CODE BEGIN ==========='
INI_AUTO_GENERATE_END = u'; =========== AUTO GENERATED CODE END ============'

CPP_BASE_FORMAT = (u"""// Auto generated code by esphomeyaml
#include "esphomelib/application.h"

using namespace esphomelib;

void setup() {
  // ===== DO NOT EDIT ANYTHING BELOW THIS LINE =====
  """, u"""
  // ========= YOU CAN EDIT AFTER THIS LINE =========
  App.setup();
}

void loop() {
  App.loop();
  delay(16);
}
""")

INI_BASE_FORMAT = (u"""; Auto generated code by esphomeyaml

[common]
lib_deps =
build_flags =
upload_flags =

; ===== DO NOT EDIT ANYTHING BELOW THIS LINE =====
""", u"""
; ========= YOU CAN EDIT AFTER THIS LINE =========

""")

INI_CONTENT_FORMAT = u"""[env:{env}]
platform = {platform}
board = {board}
framework = arduino
lib_deps =
    {lib_deps}
    ${{common.lib_deps}}
build_flags =
    {build_flags}
    ${{common.build_flags}}
upload_speed = {upload_speed}
"""

UPLOAD_SPEED_OVERRIDE = {
    'esp210': 57600,
}


def get_build_flags(config, key):
    build_flags = set()
    for _, component, conf in iter_components(config):
        if not hasattr(component, key):
            continue
        flags = getattr(component, key)
        if callable(flags):
            flags = flags(conf)
        if flags is None:
            continue
        if isinstance(flags, (str, unicode)):
            flags = [flags]
        build_flags |= set(flags)
    return build_flags


def get_ini_content(config, path):
    version_specific_settings = determine_platformio_version_settings()
    options = {
        u'env': config[CONF_ESPHOMEYAML][CONF_NAME],
        u'platform': config[CONF_ESPHOMEYAML][CONF_ARDUINO_VERSION],
        u'board': config[CONF_ESPHOMEYAML][CONF_BOARD],
        u'build_flags': u'',
        u'upload_speed': UPLOAD_SPEED_OVERRIDE.get(core.BOARD, 115200),
    }
    build_flags = set()
    if not config[CONF_ESPHOMEYAML][CONF_USE_CUSTOM_CODE]:
        build_flags |= get_build_flags(config, 'build_flags')
        build_flags |= get_build_flags(config, 'BUILD_FLAGS')
        build_flags.add(u"-DESPHOMEYAML_USE")
        build_flags.add("-Wno-unused-variable")
    build_flags |= get_build_flags(config, 'required_build_flags')
    build_flags |= get_build_flags(config, 'REQUIRED_BUILD_FLAGS')

    # avoid changing build flags order
    build_flags = sorted(list(build_flags))
    if build_flags:
        options[u'build_flags'] = u'\n    '.join(build_flags)

    lib_deps = set()

    lib_version = config[CONF_ESPHOMEYAML][CONF_ESPHOMELIB_VERSION]
    lib_path = os.path.join(path, 'lib')
    dst_path = os.path.join(lib_path, 'esphomelib')
    this_version = None
    if CONF_REPOSITORY in lib_version:
        tag = next((lib_version[x] for x in (CONF_COMMIT, CONF_BRANCH, CONF_TAG)
                    if x in lib_version), None)
        this_version = lib_version[CONF_REPOSITORY]
        if tag is not None:
            this_version += '#' + tag
        lib_deps.add(this_version)
        if os.path.islink(dst_path):
            os.unlink(dst_path)
    elif CONF_LOCAL in lib_version:
        this_version = lib_version[CONF_LOCAL]
        src_path = relative_path(this_version)
        do_write = True
        if os.path.islink(dst_path):
            old_path = os.path.join(os.readlink(dst_path), lib_path)
            if old_path != lib_path:
                os.unlink(dst_path)
            else:
                do_write = False
        if do_write:
            mkdir_p(lib_path)
            os.symlink(src_path, dst_path)

        # Manually add lib_deps because platformio seems to ignore them inside libs/
        library_json_path = os.path.join(src_path, 'library.json')
        with codecs.open(library_json_path, 'r', encoding='utf-8') as f_handle:
            library_json_text = f_handle.read()

        library_json = json.loads(library_json_text)
        for dep in library_json.get('dependencies', []):
            if 'version' in dep and VERSION_REGEX.match(dep['version']) is not None:
                lib_deps.add(dep['name'] + '@' + dep['version'])
            else:
                lib_deps.add(dep['version'])
    else:
        this_version = lib_version
        lib_deps.add(lib_version)

    version_file = os.path.join(path, '.esphomelib_version')
    version = None
    if os.path.isfile(version_file):
        with open(version_file, 'r') as ver_f:
            version = ver_f.read()

    if version != this_version:
        _LOGGER.info("Esphomelib version change detected. Cleaning build files...")
        try:
            clean_build(path)
        except OSError as err:
            _LOGGER.warn("Error deleting build files (%s)! Ignoring...", err)

        with open(version_file, 'w') as ver_f:
            ver_f.write(this_version)

    lib_deps |= get_build_flags(config, 'LIB_DEPS')
    lib_deps |= get_build_flags(config, 'lib_deps')
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        lib_deps |= {
            'Preferences',  # Preferences helper
        }
        # Manual fix for AsyncTCP
        if config[CONF_ESPHOMEYAML].get(CONF_ARDUINO_VERSION) == ARDUINO_VERSION_ESP32_DEV:
            lib_deps.add('https://github.com/me-no-dev/AsyncTCP.git#idf-update')
    # avoid changing build flags order
    lib_deps = sorted(x for x in lib_deps if x)
    if lib_deps:
        options[u'lib_deps'] = u'\n    '.join(lib_deps)

    content = INI_CONTENT_FORMAT.format(**options)
    if CONF_BOARD_FLASH_MODE in config[CONF_ESPHOMEYAML]:
        flash_mode_key = version_specific_settings['flash_mode_key']
        flash_mode = config[CONF_ESPHOMEYAML][CONF_BOARD_FLASH_MODE]
        content += "{} = {}\n".format(flash_mode_key, flash_mode)
    return content


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


def find_begin_end(text, begin_s, end_s):
    begin_index = text.find(begin_s)
    if begin_index == -1:
        raise ESPHomeYAMLError(u"Could not find auto generated code begin in file, either"
                               u"delete the main sketch file or insert the comment again.")
    if text.find(begin_s, begin_index + 1) != -1:
        raise ESPHomeYAMLError(u"Found multiple auto generate code begins, don't know"
                               u"which to chose, please remove one of them.")
    end_index = text.find(end_s)
    if end_index == -1:
        raise ESPHomeYAMLError(u"Could not find auto generated code end in file, either"
                               u"delete the main sketch file or insert the comment again.")
    if text.find(end_s, end_index + 1) != -1:
        raise ESPHomeYAMLError(u"Found multiple auto generate code endings, don't know"
                               u"which to chose, please remove one of them.")

    return text[:begin_index], text[(end_index + len(end_s)):]


def write_platformio_ini(content, path):
    if os.path.isfile(path):
        try:
            with codecs.open(path, 'r', encoding='utf-8') as f_handle:
                text = f_handle.read()
        except OSError:
            raise ESPHomeYAMLError(u"Could not read ini file at {}".format(path))
        prev_file = text
        content_format = find_begin_end(text, INI_AUTO_GENERATE_BEGIN, INI_AUTO_GENERATE_END)
    else:
        prev_file = None
        content_format = INI_BASE_FORMAT
    full_file = content_format[0] + INI_AUTO_GENERATE_BEGIN + '\n' + \
        content + INI_AUTO_GENERATE_END + content_format[1]
    if prev_file == full_file:
        return
    with codecs.open(path, mode='w+', encoding='utf-8') as f_handle:
        f_handle.write(full_file)


def write_platformio_project(config, path):
    mkdir_p(path)
    platformio_ini = os.path.join(path, 'platformio.ini')
    content = get_ini_content(config, path)
    if 'esp32_ble_beacon' in config or 'esp32_ble_tracker' in config:
        content += 'board_build.partitions = partitions.csv\n'
        partitions_csv = os.path.join(path, 'partitions.csv')
        if not os.path.isfile(partitions_csv):
            mkdir_p(path)
            with open(partitions_csv, "w") as f:
                f.write("nvs,      data, nvs,     0x009000, 0x005000,\n")
                f.write("otadata,  data, ota,     0x00e000, 0x002000,\n")
                f.write("app0,     app,  ota_0,   0x010000, 0x190000,\n")
                f.write("app1,     app,  ota_1,   0x200000, 0x190000,\n")
                f.write("eeprom,   data, 0x99,    0x390000, 0x001000,\n")
                f.write("spiffs,   data, spiffs,  0x391000, 0x00F000\n")
    write_platformio_ini(content, platformio_ini)


def write_cpp(code_s, path):
    if os.path.isfile(path):
        try:
            with codecs.open(path, 'r', encoding='utf-8') as f_handle:
                text = f_handle.read()
        except OSError:
            raise ESPHomeYAMLError(u"Could not read C++ file at {}".format(path))
        prev_file = text
        code_format = find_begin_end(text, CPP_AUTO_GENERATE_BEGIN, CPP_AUTO_GENERATE_END)
    else:
        prev_file = None
        mkdir_p(os.path.dirname(path))
        code_format = CPP_BASE_FORMAT

    full_file = code_format[0] + CPP_AUTO_GENERATE_BEGIN + '\n' + \
        code_s + CPP_AUTO_GENERATE_END + code_format[1]
    if prev_file == full_file:
        return
    with codecs.open(path, 'w+', encoding='utf-8') as f_handle:
        f_handle.write(full_file)


def determine_platformio_version_settings():
    import platformio

    settings = {}

    if platformio.VERSION < (3, 5, 3):
        settings['flash_mode_key'] = 'board_flash_mode'
    else:
        settings['flash_mode_key'] = 'board_build.flash_mode'

    return settings


def clean_build(build_path):
    for directory in ('.piolibdeps', '.pioenvs'):
        dir_path = os.path.join(build_path, directory)
        if os.path.isdir(dir_path):
            _LOGGER.info("Deleting %s", dir_path)
            shutil.rmtree(dir_path)

from __future__ import print_function

import codecs
import errno
import os

from esphomeyaml import core
from esphomeyaml.config import iter_components
from esphomeyaml.const import CONF_BOARD, CONF_BOARD_FLASH_MODE, CONF_ESPHOMEYAML, \
    CONF_LIBRARY_URI, \
    CONF_NAME, CONF_PLATFORM, CONF_USE_BUILD_FLAGS, ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError

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
"""

PLATFORM_TO_PLATFORMIO = {
    ESP_PLATFORM_ESP32: 'espressif32',
    ESP_PLATFORM_ESP8266: 'espressif8266'
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


def get_ini_content(config):
    platform = config[CONF_ESPHOMEYAML][CONF_PLATFORM]
    if platform in PLATFORM_TO_PLATFORMIO:
        platform = PLATFORM_TO_PLATFORMIO[platform]
    options = {
        u'env': config[CONF_ESPHOMEYAML][CONF_NAME],
        u'platform': platform,
        u'board': config[CONF_ESPHOMEYAML][CONF_BOARD],
        u'build_flags': u'',
    }
    build_flags = set()
    if config[CONF_ESPHOMEYAML][CONF_USE_BUILD_FLAGS]:
        build_flags |= get_build_flags(config, 'build_flags')
        build_flags |= get_build_flags(config, 'BUILD_FLAGS')
        build_flags.add(u"-DESPHOMEYAML_USE")
    build_flags |= get_build_flags(config, 'required_build_flags')
    build_flags |= get_build_flags(config, 'REQUIRED_BUILD_FLAGS')

    # avoid changing build flags order
    build_flags = sorted(list(build_flags))
    if build_flags:
        options[u'build_flags'] = u'\n    '.join(build_flags)

    lib_deps = set()
    lib_deps.add(config[CONF_ESPHOMEYAML][CONF_LIBRARY_URI])
    lib_deps |= get_build_flags(config, 'LIB_DEPS')
    lib_deps |= get_build_flags(config, 'lib_deps')
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        lib_deps |= {
            'Preferences',  # Preferences helper
        }
    # avoid changing build flags order
    lib_deps = sorted(x for x in lib_deps if x)
    if lib_deps:
        options[u'lib_deps'] = u'\n    '.join(lib_deps)

    content = INI_CONTENT_FORMAT.format(**options)
    if CONF_BOARD_FLASH_MODE in config[CONF_ESPHOMEYAML]:
        flash_mode = config[CONF_ESPHOMEYAML][CONF_BOARD_FLASH_MODE]
        content += "board_flash_mode = {}\n".format(flash_mode)
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
        mkdir_p(os.path.dirname(path))
        content_format = INI_BASE_FORMAT
    full_file = content_format[0] + INI_AUTO_GENERATE_BEGIN + '\n' + \
        content + INI_AUTO_GENERATE_END + content_format[1]
    if prev_file == full_file:
        return
    with codecs.open(path, mode='w+', encoding='utf-8') as f_handle:
        f_handle.write(full_file)


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

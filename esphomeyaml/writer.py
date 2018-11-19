from __future__ import print_function

import codecs
import json
import logging
import os
import re
import shutil

from esphomeyaml.config import iter_components
from esphomeyaml.const import ARDUINO_VERSION_ESP32_DEV, CONF_ARDUINO_VERSION, \
    CONF_BOARD_FLASH_MODE, CONF_BRANCH, CONF_COMMIT, CONF_ESPHOMELIB_VERSION, CONF_ESPHOMEYAML, \
    CONF_LOCAL, CONF_REPOSITORY, CONF_TAG, CONF_USE_CUSTOM_CODE
from esphomeyaml.core import CORE, EsphomeyamlError
from esphomeyaml.core_config import VERSION_REGEX
from esphomeyaml.helpers import mkdir_p, run_system_command
from esphomeyaml.storage_json import StorageJSON, storage_path
from esphomeyaml.util import safe_print

_LOGGER = logging.getLogger(__name__)

CPP_AUTO_GENERATE_BEGIN = u'// ========== AUTO GENERATED CODE BEGIN ==========='
CPP_AUTO_GENERATE_END = u'// =========== AUTO GENERATED CODE END ============'
CPP_INCLUDE_BEGIN = u'// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ==========='
CPP_INCLUDE_END = u'// ========== AUTO GENERATED INCLUDE BLOCK END ==========='
INI_AUTO_GENERATE_BEGIN = u'; ========== AUTO GENERATED CODE BEGIN ==========='
INI_AUTO_GENERATE_END = u'; =========== AUTO GENERATED CODE END ============'

CPP_BASE_FORMAT = (u"""// Auto generated code by esphomeyaml
""", u""""

void setup() {
  // ===== DO NOT EDIT ANYTHING BELOW THIS LINE =====
  """, u"""
  // ========= YOU CAN EDIT AFTER THIS LINE =========
  App.setup();
}

void loop() {
  App.loop();
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


def get_build_flags(key):
    build_flags = set()
    for _, component, conf in iter_components(CORE.config):
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


def get_include_text():
    include_text = u'#include "esphomelib/application.h"\n' \
                   u'using namespace esphomelib;\n'
    for _, component, conf in iter_components(CORE.config):
        if not hasattr(component, 'includes'):
            continue
        includes = component.includes
        if callable(includes):
            includes = includes(conf)
        if includes is None:
            continue
        if isinstance(includes, list):
            includes = '\n'.join(includes)
        if not includes:
            continue
        include_text += includes + '\n'
    return include_text


def update_esphomelib_repo():
    if CONF_REPOSITORY not in CORE.esphomelib_version:
        return

    if CONF_BRANCH not in CORE.esphomelib_version:
        # Git commit hash or tag cannot be updated
        return

    esphomelib_path = CORE.relative_build_path('.piolibdeps', 'esphomelib')

    rc, _, _ = run_system_command('git', '-C', esphomelib_path, '--help')
    if rc != 0:
        # git not installed or repo not downloaded yet
        return
    rc, _, _ = run_system_command('git', '-C', esphomelib_path, 'diff-index', '--quiet', 'HEAD',
                                  '--')
    if rc != 0:
        # local changes, cannot update
        _LOGGER.warn("Local changes in esphomelib copy from git. Will not auto-update.")
        return
    _LOGGER.info("Updating esphomelib copy from git (%s)", esphomelib_path)
    rc, stdout, _ = run_system_command('git', '-c', 'color.ui=always', '-C', esphomelib_path,
                                       'pull', '--stat')
    if rc != 0:
        _LOGGER.warn("Couldn't auto-update local git copy of esphomelib.")
        return
    safe_print(stdout.strip())


def replace_file_content(text, pattern, repl):
    content_new, count = re.subn(pattern, repl, text, flags=re.M)
    return content_new, count


def migrate_src_version_0_to_1():
    main_cpp = CORE.relative_build_path('src', 'main.cpp')
    with codecs.open(main_cpp, 'r', encoding='utf-8') as f_handle:
        content = orig_content = f_handle.read()

    content, count = replace_file_content(content, r'\s*delay\((?:16|20)\);', '')
    if count != 0:
        _LOGGER.info("Migration: Removed %s occurrence of 'delay(16);' in %s", count, main_cpp)

    content, count = replace_file_content(content, r'using namespace esphomelib;', '')
    if count != 0:
        _LOGGER.info("Migration: Removed %s occurrence of 'using namespace esphomelib;' "
                     "in %s", count, main_cpp)

    if CPP_INCLUDE_BEGIN not in content:
        content, count = replace_file_content(content, r'#include "esphomelib/application.h"',
                                              CPP_INCLUDE_BEGIN + u'\n' + CPP_INCLUDE_END)
        if count == 0:
            _LOGGER.error("Migration failed. esphomeyaml 1.10.0 needs to have a new auto-generated "
                          "include section in the %s file. Please remove %s and let it be "
                          "auto-generated again.", main_cpp, main_cpp)
        _LOGGER.info("Migration: Added include section to %s", main_cpp)

    if orig_content == content:
        return
    with codecs.open(main_cpp, 'w', encoding='utf-8') as f_handle:
        f_handle.write(content)


def migrate_src_version(old, new):
    if old == new:
        return
    if old > new:
        _LOGGER.warning("The source version rolled backwards! Ignoring.")
        return

    if old == 0:
        migrate_src_version_0_to_1()


def update_storage_json():
    path = storage_path()
    old = StorageJSON.load(path)
    new = StorageJSON.from_esphomeyaml_core(CORE)
    if old == new:
        return

    old_src_version = old.src_version if old is not None else 0
    migrate_src_version(old_src_version, new.src_version)

    _LOGGER.info("Core config or version changed, cleaning build files...")
    clean_build()

    new.save(path)


def symlink_esphomelib_version(esphomelib_version):
    lib_path = CORE.relative_build_path('lib')
    dst_path = CORE.relative_build_path('lib', 'esphomelib')
    if CORE.is_local_esphomelib_copy:
        src_path = CORE.relative_path(esphomelib_version[CONF_LOCAL])
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
    else:
        # Remove symlink when changing back from local version
        if os.path.islink(dst_path):
            os.unlink(dst_path)


def gather_lib_deps():
    lib_deps = set()
    esphomelib_version = CORE.config[CONF_ESPHOMEYAML][CONF_ESPHOMELIB_VERSION]
    if CONF_REPOSITORY in esphomelib_version:
        ref = next((esphomelib_version[x] for x in (CONF_COMMIT, CONF_BRANCH, CONF_TAG)
                    if x in esphomelib_version), None)
        this_version = esphomelib_version[CONF_REPOSITORY]
        if ref is not None:
            this_version += '#' + ref
        lib_deps.add(this_version)
    elif CORE.is_local_esphomelib_copy:
        src_path = CORE.relative_path(esphomelib_version[CONF_LOCAL])
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
        lib_deps.add(esphomelib_version)

    lib_deps |= get_build_flags('LIB_DEPS')
    lib_deps |= get_build_flags('lib_deps')
    if CORE.is_esp32:
        lib_deps |= {
            'Preferences',  # Preferences helper
        }
        # Manual fix for AsyncTCP
        if CORE.config[CONF_ESPHOMEYAML].get(CONF_ARDUINO_VERSION) == ARDUINO_VERSION_ESP32_DEV:
            lib_deps.add('https://github.com/me-no-dev/AsyncTCP.git#idf-update')
    # avoid changing build flags order
    return sorted(x for x in lib_deps if x)


def gather_build_flags():
    build_flags = set()
    if not CORE.config[CONF_ESPHOMEYAML][CONF_USE_CUSTOM_CODE]:
        build_flags |= get_build_flags('build_flags')
        build_flags |= get_build_flags('BUILD_FLAGS')
        build_flags.add('-DESPHOMEYAML_USE')
        build_flags.add("-Wno-unused-variable")
        build_flags.add("-Wno-unused-but-set-variable")
    build_flags |= get_build_flags('required_build_flags')
    build_flags |= get_build_flags('REQUIRED_BUILD_FLAGS')

    # avoid changing build flags order
    return sorted(list(build_flags))


def get_ini_content():
    version_specific_settings = determine_platformio_version_settings()
    options = {
        u'env': CORE.name,
        u'platform': CORE.config[CONF_ESPHOMEYAML][CONF_ARDUINO_VERSION],
        u'board': CORE.board,
        u'build_flags': u'\n    '.join(gather_build_flags()),
        u'upload_speed': UPLOAD_SPEED_OVERRIDE.get(CORE.board, 115200),
        u'lib_deps': u'\n    '.join(gather_lib_deps()),
    }
    content = INI_CONTENT_FORMAT.format(**options)
    if CONF_BOARD_FLASH_MODE in CORE.config[CONF_ESPHOMEYAML]:
        flash_mode_key = version_specific_settings['flash_mode_key']
        flash_mode = CORE.config[CONF_ESPHOMEYAML][CONF_BOARD_FLASH_MODE]
        content += "{} = {}\n".format(flash_mode_key, flash_mode)
    return content


def find_begin_end(text, begin_s, end_s):
    begin_index = text.find(begin_s)
    if begin_index == -1:
        raise EsphomeyamlError(u"Could not find auto generated code begin in file, either "
                               u"delete the main sketch file or insert the comment again.")
    if text.find(begin_s, begin_index + 1) != -1:
        raise EsphomeyamlError(u"Found multiple auto generate code begins, don't know "
                               u"which to chose, please remove one of them.")
    end_index = text.find(end_s)
    if end_index == -1:
        raise EsphomeyamlError(u"Could not find auto generated code end in file, either "
                               u"delete the main sketch file or insert the comment again.")
    if text.find(end_s, end_index + 1) != -1:
        raise EsphomeyamlError(u"Found multiple auto generate code endings, don't know "
                               u"which to chose, please remove one of them.")

    return text[:begin_index], text[(end_index + len(end_s)):]


def write_platformio_ini(content, path):
    symlink_esphomelib_version(CORE.esphomelib_version)
    update_esphomelib_repo()
    update_storage_json()

    if os.path.isfile(path):
        try:
            with codecs.open(path, 'r', encoding='utf-8') as f_handle:
                text = f_handle.read()
        except OSError:
            raise EsphomeyamlError(u"Could not read ini file at {}".format(path))
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


def write_platformio_project():
    mkdir_p(CORE.build_path)

    platformio_ini = CORE.relative_build_path('platformio.ini')
    content = get_ini_content()
    if 'esp32_ble_beacon' in CORE.config or 'esp32_ble_tracker' in CORE.config:
        content += 'board_build.partitions = partitions.csv\n'
        partitions_csv = CORE.relative_build_path('partitions.csv')
        if not os.path.isfile(partitions_csv):
            with open(partitions_csv, "w") as f:
                f.write("nvs,      data, nvs,     0x009000, 0x005000,\n")
                f.write("otadata,  data, ota,     0x00e000, 0x002000,\n")
                f.write("app0,     app,  ota_0,   0x010000, 0x190000,\n")
                f.write("app1,     app,  ota_1,   0x200000, 0x190000,\n")
                f.write("eeprom,   data, 0x99,    0x390000, 0x001000,\n")
                f.write("spiffs,   data, spiffs,  0x391000, 0x00F000\n")
    write_platformio_ini(content, platformio_ini)


def write_cpp(code_s):
    path = CORE.relative_build_path('src', 'main.cpp')
    if os.path.isfile(path):
        try:
            with codecs.open(path, 'r', encoding='utf-8') as f_handle:
                text = f_handle.read()
        except OSError:
            raise EsphomeyamlError(u"Could not read C++ file at {}".format(path))
        prev_file = text
        code_format = find_begin_end(text, CPP_AUTO_GENERATE_BEGIN, CPP_AUTO_GENERATE_END)
        code_format_ = find_begin_end(code_format[0], CPP_INCLUDE_BEGIN, CPP_INCLUDE_END)
        code_format = (code_format_[0], code_format_[1], code_format[1])
    else:
        prev_file = None
        mkdir_p(os.path.dirname(path))
        code_format = CPP_BASE_FORMAT

    include_s = get_include_text()

    full_file = code_format[0] + CPP_INCLUDE_BEGIN + u'\n' + include_s + CPP_INCLUDE_END + \
                code_format[1] + CPP_AUTO_GENERATE_BEGIN + u'\n' + code_s + \
                CPP_AUTO_GENERATE_END + code_format[2]
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


def clean_build():
    for directory in ('.piolibdeps', '.pioenvs'):
        dir_path = CORE.relative_build_path(directory)
        if not os.path.isdir(dir_path):
            continue
        _LOGGER.info("Deleting %s", dir_path)
        shutil.rmtree(dir_path)

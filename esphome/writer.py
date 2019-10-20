from __future__ import print_function

import logging
import os
import re

from esphome.config import iter_components
from esphome.const import CONF_BOARD_FLASH_MODE, CONF_ESPHOME, CONF_PLATFORMIO_OPTIONS, \
    HEADER_FILE_EXTENSIONS, SOURCE_FILE_EXTENSIONS, __version__
from esphome.core import CORE, EsphomeError
from esphome.helpers import mkdir_p, read_file, write_file_if_changed, walk_files
from esphome.storage_json import StorageJSON, storage_path

_LOGGER = logging.getLogger(__name__)

CPP_AUTO_GENERATE_BEGIN = u'// ========== AUTO GENERATED CODE BEGIN ==========='
CPP_AUTO_GENERATE_END = u'// =========== AUTO GENERATED CODE END ============'
CPP_INCLUDE_BEGIN = u'// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ==========='
CPP_INCLUDE_END = u'// ========== AUTO GENERATED INCLUDE BLOCK END ==========='
INI_AUTO_GENERATE_BEGIN = u'; ========== AUTO GENERATED CODE BEGIN ==========='
INI_AUTO_GENERATE_END = u'; =========== AUTO GENERATED CODE END ============'

CPP_BASE_FORMAT = (u"""// Auto generated code by esphome
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

INI_BASE_FORMAT = (u"""; Auto generated code by esphome

[common]
lib_deps =
build_flags =
upload_flags =

; ===== DO NOT EDIT ANYTHING BELOW THIS LINE =====
""", u"""
; ========= YOU CAN EDIT AFTER THIS LINE =========

""")

UPLOAD_SPEED_OVERRIDE = {
    'esp210': 57600,
}


def get_flags(key):
    flags = set()
    for _, component, conf in iter_components(CORE.config):
        flags |= getattr(component, key)(conf)
    return flags


def get_include_text():
    include_text = u'#include "esphome.h"\n' \
                   u'using namespace esphome;\n'
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


def replace_file_content(text, pattern, repl):
    content_new, count = re.subn(pattern, repl, text, flags=re.M)
    return content_new, count


def migrate_src_version_0_to_1():
    main_cpp = CORE.relative_build_path('src', 'main.cpp')
    if not os.path.isfile(main_cpp):
        return

    content = read_file(main_cpp)

    if CPP_INCLUDE_BEGIN in content:
        return

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
            _LOGGER.error("Migration failed. ESPHome 1.10.0 needs to have a new auto-generated "
                          "include section in the %s file. Please remove %s and let it be "
                          "auto-generated again.", main_cpp, main_cpp)
        _LOGGER.info("Migration: Added include section to %s", main_cpp)

    write_file_if_changed(content, main_cpp)


def migrate_src_version(old, new):
    if old == new:
        return
    if old > new:
        _LOGGER.warning("The source version rolled backwards! Ignoring.")
        return

    if old == 0:
        migrate_src_version_0_to_1()


def storage_should_clean(old, new):  # type: (StorageJSON, StorageJSON) -> bool
    if old is None:
        return True

    if old.src_version != new.src_version:
        return True
    if old.arduino_version != new.arduino_version:
        return True
    if old.board != new.board:
        return True
    if old.build_path != new.build_path:
        return True
    return False


def update_storage_json():
    path = storage_path()
    old = StorageJSON.load(path)
    new = StorageJSON.from_esphome_core(CORE, old)
    if old == new:
        return

    old_src_version = old.src_version if old is not None else 0
    migrate_src_version(old_src_version, new.src_version)

    if storage_should_clean(old, new):
        _LOGGER.info("Core config or version changed, cleaning build files...")
        clean_build()

    new.save(path)


def format_ini(data):
    content = u''
    for key, value in sorted(data.items()):
        if isinstance(value, (list, set, tuple)):
            content += u'{} =\n'.format(key)
            for x in value:
                content += u'    {}\n'.format(x)
        else:
            content += u'{} = {}\n'.format(key, value)
    return content


def gather_lib_deps():
    return [x.as_lib_dep for x in CORE.libraries]


def gather_build_flags():
    build_flags = CORE.build_flags

    # avoid changing build flags order
    return list(sorted(list(build_flags)))


def get_ini_content():
    lib_deps = gather_lib_deps()
    build_flags = gather_build_flags()

    data = {
        'platform': CORE.arduino_version,
        'board': CORE.board,
        'framework': 'arduino',
        'lib_deps': lib_deps + ['${common.lib_deps}'],
        'build_flags': build_flags + ['${common.build_flags}'],
        'upload_speed': UPLOAD_SPEED_OVERRIDE.get(CORE.board, 460800),
    }

    if CORE.is_esp32:
        data['board_build.partitions'] = "partitions.csv"
        partitions_csv = CORE.relative_build_path('partitions.csv')
        if not os.path.isfile(partitions_csv):
            with open(partitions_csv, "w") as f:
                f.write("nvs,      data, nvs,     0x009000, 0x005000,\n")
                f.write("otadata,  data, ota,     0x00e000, 0x002000,\n")
                f.write("app0,     app,  ota_0,   0x010000, 0x190000,\n")
                f.write("app1,     app,  ota_1,   0x200000, 0x190000,\n")
                f.write("eeprom,   data, 0x99,    0x390000, 0x001000,\n")
                f.write("spiffs,   data, spiffs,  0x391000, 0x00F000\n")

    if CONF_BOARD_FLASH_MODE in CORE.config[CONF_ESPHOME]:
        flash_mode = CORE.config[CONF_ESPHOME][CONF_BOARD_FLASH_MODE]
        data['board_build.flash_mode'] = flash_mode

    # Ignore libraries that are not explicitly used, but may
    # be added by LDF
    # data['lib_ldf_mode'] = 'chain'
    data.update(CORE.config[CONF_ESPHOME].get(CONF_PLATFORMIO_OPTIONS, {}))

    content = u'[env:{}]\n'.format(CORE.name)
    content += format_ini(data)

    return content


def find_begin_end(text, begin_s, end_s):
    begin_index = text.find(begin_s)
    if begin_index == -1:
        raise EsphomeError(u"Could not find auto generated code begin in file, either "
                           u"delete the main sketch file or insert the comment again.")
    if text.find(begin_s, begin_index + 1) != -1:
        raise EsphomeError(u"Found multiple auto generate code begins, don't know "
                           u"which to chose, please remove one of them.")
    end_index = text.find(end_s)
    if end_index == -1:
        raise EsphomeError(u"Could not find auto generated code end in file, either "
                           u"delete the main sketch file or insert the comment again.")
    if text.find(end_s, end_index + 1) != -1:
        raise EsphomeError(u"Found multiple auto generate code endings, don't know "
                           u"which to chose, please remove one of them.")

    return text[:begin_index], text[(end_index + len(end_s)):]


def write_platformio_ini(content):
    update_storage_json()
    path = CORE.relative_build_path('platformio.ini')

    if os.path.isfile(path):
        text = read_file(path)
        content_format = find_begin_end(text, INI_AUTO_GENERATE_BEGIN, INI_AUTO_GENERATE_END)
    else:
        content_format = INI_BASE_FORMAT
    full_file = content_format[0] + INI_AUTO_GENERATE_BEGIN + '\n' + content
    full_file += INI_AUTO_GENERATE_END + content_format[1]
    write_file_if_changed(full_file, path)


def write_platformio_project():
    mkdir_p(CORE.build_path)

    content = get_ini_content()
    write_gitignore()
    write_platformio_ini(content)


DEFINES_H_FORMAT = ESPHOME_H_FORMAT = u"""\
#pragma once
{}
"""
VERSION_H_FORMAT = u"""\
#pragma once
#define ESPHOME_VERSION "{}"
"""
DEFINES_H_TARGET = 'esphome/core/defines.h'
VERSION_H_TARGET = 'esphome/core/version.h'
ESPHOME_README_TXT = u"""
THIS DIRECTORY IS AUTO-GENERATED, DO NOT MODIFY

ESPHome automatically populates the esphome/ directory, and any
changes to this directory will be removed the next time esphome is
run.

For modifying esphome's core files, please use a development esphome install
or use the custom_components folder.
"""


def copy_src_tree():
    import filecmp
    import shutil

    source_files = {}
    for _, component, _ in iter_components(CORE.config):
        source_files.update(component.source_files)

    # Convert to list and sort
    source_files_l = [it for it in source_files.items()]
    source_files_l.sort()

    # Build #include list for esphome.h
    include_l = []
    for target, path in source_files_l:
        if os.path.splitext(path)[1] in HEADER_FILE_EXTENSIONS:
            include_l.append(u'#include "{}"'.format(target))
    include_l.append(u'')
    include_s = u'\n'.join(include_l)

    source_files_copy = source_files.copy()
    source_files_copy.pop(DEFINES_H_TARGET)

    for path in walk_files(CORE.relative_src_path('esphome')):
        if os.path.splitext(path)[1] not in SOURCE_FILE_EXTENSIONS:
            # Not a source file, ignore
            continue
        # Transform path to target path name
        target = os.path.relpath(path, CORE.relative_src_path()).replace(os.path.sep, '/')
        if target in (DEFINES_H_TARGET, VERSION_H_TARGET):
            # Ignore defines.h, will be dealt with later
            continue
        if target not in source_files_copy:
            # Source file removed, delete target
            os.remove(path)
        else:
            src_path = source_files_copy.pop(target)
            if not filecmp.cmp(path, src_path):
                # Files are not same, copy
                shutil.copy(src_path, path)

    # Now copy new files
    for target, src_path in source_files_copy.items():
        dst_path = CORE.relative_src_path(*target.split('/'))
        mkdir_p(os.path.dirname(dst_path))
        shutil.copy(src_path, dst_path)

    # Finally copy defines
    write_file_if_changed(generate_defines_h(),
                          CORE.relative_src_path('esphome', 'core', 'defines.h'))
    write_file_if_changed(ESPHOME_README_TXT,
                          CORE.relative_src_path('esphome', 'README.txt'))
    write_file_if_changed(ESPHOME_H_FORMAT.format(include_s),
                          CORE.relative_src_path('esphome.h'))
    write_file_if_changed(VERSION_H_FORMAT.format(__version__),
                          CORE.relative_src_path('esphome', 'core', 'version.h'))


def generate_defines_h():
    define_content_l = [x.as_macro for x in CORE.defines]
    define_content_l.sort()
    return DEFINES_H_FORMAT.format(u'\n'.join(define_content_l))


def write_cpp(code_s):
    path = CORE.relative_src_path('main.cpp')
    if os.path.isfile(path):
        text = read_file(path)
        code_format = find_begin_end(text, CPP_AUTO_GENERATE_BEGIN, CPP_AUTO_GENERATE_END)
        code_format_ = find_begin_end(code_format[0], CPP_INCLUDE_BEGIN, CPP_INCLUDE_END)
        code_format = (code_format_[0], code_format_[1], code_format[1])
    else:
        code_format = CPP_BASE_FORMAT

    copy_src_tree()
    global_s = u'#include "esphome.h"\n'
    global_s += CORE.cpp_global_section

    full_file = code_format[0] + CPP_INCLUDE_BEGIN + u'\n' + global_s + CPP_INCLUDE_END
    full_file += code_format[1] + CPP_AUTO_GENERATE_BEGIN + u'\n' + code_s + CPP_AUTO_GENERATE_END
    full_file += code_format[2]
    write_file_if_changed(full_file, path)


def clean_build():
    import shutil

    pioenvs = CORE.relative_pioenvs_path()
    if os.path.isdir(pioenvs):
        _LOGGER.info("Deleting %s", pioenvs)
        shutil.rmtree(pioenvs)
    piolibdeps = CORE.relative_piolibdeps_path()
    if os.path.isdir(piolibdeps):
        _LOGGER.info("Deleting %s", piolibdeps)
        shutil.rmtree(piolibdeps)


GITIGNORE_CONTENT = """# Gitignore settings for ESPHome
# This is an example and may include too much for your use-case.
# You can modify this file to suit your needs.
/.esphome/
**/.pioenvs/
**/.piolibdeps/
**/lib/
**/src/
**/platformio.ini
/secrets.yaml
"""


def write_gitignore():
    path = CORE.relative_config_path('.gitignore')
    if not os.path.isfile(path):
        with open(path, 'w') as f:
            f.write(GITIGNORE_CONTENT)

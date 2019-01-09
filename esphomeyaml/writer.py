from __future__ import print_function

import codecs
import json
import logging
import os
import re
import shutil

from esphomeyaml.config import iter_components
from esphomeyaml.const import ARDUINO_VERSION_ESP32_DEV, CONF_ARDUINO_VERSION, \
    CONF_BRANCH, CONF_COMMIT, CONF_ESPHOMELIB_VERSION, CONF_ESPHOMEYAML, \
    CONF_LOCAL, CONF_REPOSITORY, CONF_TAG, CONF_USE_CUSTOM_CODE, CONF_PLATFORMIO_OPTIONS, \
    CONF_BOARD_FLASH_MODE, ARDUINO_VERSION_ESP8266_DEV
from esphomeyaml.core import CORE, EsphomeyamlError
from esphomeyaml.core_config import VERSION_REGEX, LIBRARY_URI_REPO, GITHUB_ARCHIVE_ZIP
from esphomeyaml.helpers import mkdir_p, run_system_command
from esphomeyaml.pins import ESP8266_LD_SCRIPTS, ESP8266_FLASH_SIZES
from esphomeyaml.py_compat import IS_PY3, string_types
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
        if isinstance(flags, string_types):
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
        _LOGGER.warning("Local changes in esphomelib copy from git. Will not auto-update.")
        return
    _LOGGER.info("Updating esphomelib copy from git (%s)", esphomelib_path)
    rc, stdout, _ = run_system_command('git', '-c', 'color.ui=always', '-C', esphomelib_path,
                                       'pull', '--stat')
    if rc != 0:
        _LOGGER.warning("Couldn't auto-update local git copy of esphomelib.")
        return
    if IS_PY3:
        stdout = stdout.decode('utf-8', 'backslashreplace')
    safe_print(stdout.strip())


def replace_file_content(text, pattern, repl):
    content_new, count = re.subn(pattern, repl, text, flags=re.M)
    return content_new, count


def migrate_src_version_0_to_1():
    main_cpp = CORE.relative_build_path('src', 'main.cpp')
    if not os.path.isfile(main_cpp):
        return

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


def storage_should_clean(old, new):  # type: (StorageJSON, StorageJSON) -> bool
    if old is None:
        return True

    if old.esphomelib_version != new.esphomelib_version:
        return True
    if old.esphomeyaml_version != new.esphomeyaml_version:
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
    new = StorageJSON.from_esphomeyaml_core(CORE, old)
    if old == new:
        return

    old_src_version = old.src_version if old is not None else 0
    migrate_src_version(old_src_version, new.src_version)

    if storage_should_clean(old, new):
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
    lib_deps = set()
    esphomelib_version = CORE.config[CONF_ESPHOMEYAML][CONF_ESPHOMELIB_VERSION]
    if CONF_REPOSITORY in esphomelib_version:
        repo = esphomelib_version[CONF_REPOSITORY]
        ref = next((esphomelib_version[x] for x in (CONF_COMMIT, CONF_BRANCH, CONF_TAG)
                    if x in esphomelib_version), None)
        if CONF_TAG in esphomelib_version and repo == LIBRARY_URI_REPO:
            this_version = GITHUB_ARCHIVE_ZIP.format(ref)
        elif ref is not None:
            this_version = repo + '#' + ref
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
        lib_deps.add('esphomelib')
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
            lib_deps.discard('AsyncTCP@1.0.1')
    # avoid changing build flags order
    return list(sorted(x for x in lib_deps if x))


def gather_build_flags():
    build_flags = set()
    if not CORE.config[CONF_ESPHOMEYAML][CONF_USE_CUSTOM_CODE]:
        build_flags |= get_build_flags('build_flags')
        build_flags |= get_build_flags('BUILD_FLAGS')
        build_flags.add('-DESPHOMEYAML_USE')
        build_flags.add("-Wno-unused-variable")
        build_flags.add("-Wno-unused-but-set-variable")
        build_flags.add("-Wno-sign-compare")
    build_flags |= get_build_flags('required_build_flags')
    build_flags |= get_build_flags('REQUIRED_BUILD_FLAGS')

    # avoid changing build flags order
    return list(sorted(list(build_flags)))


def get_ini_content():
    lib_deps = gather_lib_deps()
    build_flags = gather_build_flags()

    if CORE.is_esp8266 and CORE.board in ESP8266_FLASH_SIZES:
        flash_size = ESP8266_FLASH_SIZES[CORE.board]
        ld_scripts = ESP8266_LD_SCRIPTS[flash_size]
        ld_script = None

        if CORE.arduino_version in ('espressif8266@1.8.0', 'espressif8266@1.7.3',
                                    'espressif8266@1.6.0', 'espressif8266@1.5.0'):
            ld_script = ld_scripts[0]
        elif CORE.arduino_version == ARDUINO_VERSION_ESP8266_DEV:
            ld_script = ld_scripts[1]

        if ld_script is not None:
            build_flags.append('-Wl,-T{}'.format(ld_script))

    data = {
        'platform': CORE.config[CONF_ESPHOMEYAML][CONF_ARDUINO_VERSION],
        'board': CORE.board,
        'framework': 'arduino',
        'lib_deps': lib_deps + ['${common.lib_deps}'],
        'build_flags': build_flags + ['${common.build_flags}'],
        'upload_speed': UPLOAD_SPEED_OVERRIDE.get(CORE.board, 115200),
    }

    if 'esp32_ble_beacon' in CORE.config or 'esp32_ble_tracker' in CORE.config:
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

    if CONF_BOARD_FLASH_MODE in CORE.config[CONF_ESPHOMEYAML]:
        flash_mode = CORE.config[CONF_ESPHOMEYAML][CONF_BOARD_FLASH_MODE]
        data['board_build.flash_mode'] = flash_mode

    data.update(CORE.config[CONF_ESPHOMEYAML].get(CONF_PLATFORMIO_OPTIONS, {}))

    content = u'[env:{}]\n'.format(CORE.name)
    content += format_ini(data)

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
    full_file = content_format[0] + INI_AUTO_GENERATE_BEGIN + '\n' + content
    full_file += INI_AUTO_GENERATE_END + content_format[1]
    if prev_file == full_file:
        return
    with codecs.open(path, mode='w+', encoding='utf-8') as f_handle:
        f_handle.write(full_file)


def write_platformio_project():
    mkdir_p(CORE.build_path)

    platformio_ini = CORE.relative_build_path('platformio.ini')
    content = get_ini_content()
    write_gitignore()
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

    full_file = code_format[0] + CPP_INCLUDE_BEGIN + u'\n' + include_s + CPP_INCLUDE_END
    full_file += code_format[1] + CPP_AUTO_GENERATE_BEGIN + u'\n' + code_s + CPP_AUTO_GENERATE_END
    full_file += code_format[2]
    if prev_file == full_file:
        return
    with codecs.open(path, 'w+', encoding='utf-8') as f_handle:
        f_handle.write(full_file)


def clean_build():
    for directory in ('.piolibdeps', '.pioenvs'):
        dir_path = CORE.relative_build_path(directory)
        if not os.path.isdir(dir_path):
            continue
        _LOGGER.info("Deleting %s", dir_path)
        shutil.rmtree(dir_path)


GITIGNORE_CONTENT = """# Gitignore settings for esphomeyaml
# This is an example and may include too much for your use-case.
# You can modify this file to suit your needs.
/.esphomeyaml/
**/.pioenvs/
**/.piolibdeps/
**/lib/
**/src/
**/platformio.ini
/secrets.yaml
"""


def write_gitignore():
    path = CORE.relative_path('.gitignore')
    if not os.path.isfile(path):
        with open(path, 'w') as f:
            f.write(GITIGNORE_CONTENT)

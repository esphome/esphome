import logging
import os
import re
from pathlib import Path
from typing import Dict

from esphome.config import iter_components
from esphome.const import (
    CONF_BOARD_FLASH_MODE,
    CONF_ESPHOME,
    CONF_PLATFORMIO_OPTIONS,
    HEADER_FILE_EXTENSIONS,
    SOURCE_FILE_EXTENSIONS,
    __version__,
    ARDUINO_VERSION_ESP8266,
    ENV_NOGITIGNORE,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import (
    mkdir_p,
    read_file,
    write_file_if_changed,
    walk_files,
    copy_file_if_changed,
    get_bool_env,
)
from esphome.storage_json import StorageJSON, storage_path
from esphome.pins import ESP8266_FLASH_SIZES, ESP8266_LD_SCRIPTS
from esphome import loader

_LOGGER = logging.getLogger(__name__)

CPP_AUTO_GENERATE_BEGIN = "// ========== AUTO GENERATED CODE BEGIN ==========="
CPP_AUTO_GENERATE_END = "// =========== AUTO GENERATED CODE END ============"
CPP_INCLUDE_BEGIN = "// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ==========="
CPP_INCLUDE_END = "// ========== AUTO GENERATED INCLUDE BLOCK END ==========="
INI_AUTO_GENERATE_BEGIN = "; ========== AUTO GENERATED CODE BEGIN ==========="
INI_AUTO_GENERATE_END = "; =========== AUTO GENERATED CODE END ============"

CPP_BASE_FORMAT = (
    """// Auto generated code by esphome
""",
    """"

void setup() {
  // ===== DO NOT EDIT ANYTHING BELOW THIS LINE =====
  """,
    """
  // ========= YOU CAN EDIT AFTER THIS LINE =========
  App.setup();
}

void loop() {
  App.loop();
}
""",
)

INI_BASE_FORMAT = (
    """; Auto generated code by esphome

[common]
lib_deps =
build_flags =
upload_flags =

; ===== DO NOT EDIT ANYTHING BELOW THIS LINE =====
""",
    """
; ========= YOU CAN EDIT AFTER THIS LINE =========

""",
)

UPLOAD_SPEED_OVERRIDE = {
    "esp210": 57600,
}


def get_flags(key):
    flags = set()
    for _, component, conf in iter_components(CORE.config):
        flags |= getattr(component, key)(conf)
    return flags


def get_include_text():
    include_text = '#include "esphome.h"\nusing namespace esphome;\n'
    for _, component, conf in iter_components(CORE.config):
        if not hasattr(component, "includes"):
            continue
        includes = component.includes
        if callable(includes):
            includes = includes(conf)
        if includes is None:
            continue
        if isinstance(includes, list):
            includes = "\n".join(includes)
        if not includes:
            continue
        include_text += includes + "\n"
    return include_text


def replace_file_content(text, pattern, repl):
    content_new, count = re.subn(pattern, repl, text, flags=re.M)
    return content_new, count


def migrate_src_version_0_to_1():
    main_cpp = CORE.relative_build_path("src", "main.cpp")
    if not os.path.isfile(main_cpp):
        return

    content = read_file(main_cpp)

    if CPP_INCLUDE_BEGIN in content:
        return

    content, count = replace_file_content(content, r"\s*delay\((?:16|20)\);", "")
    if count != 0:
        _LOGGER.info(
            "Migration: Removed %s occurrence of 'delay(16);' in %s", count, main_cpp
        )

    content, count = replace_file_content(content, r"using namespace esphomelib;", "")
    if count != 0:
        _LOGGER.info(
            "Migration: Removed %s occurrence of 'using namespace esphomelib;' "
            "in %s",
            count,
            main_cpp,
        )

    if CPP_INCLUDE_BEGIN not in content:
        content, count = replace_file_content(
            content,
            r'#include "esphomelib/application.h"',
            CPP_INCLUDE_BEGIN + "\n" + CPP_INCLUDE_END,
        )
        if count == 0:
            _LOGGER.error(
                "Migration failed. ESPHome 1.10.0 needs to have a new auto-generated "
                "include section in the %s file. Please remove %s and let it be "
                "auto-generated again.",
                main_cpp,
                main_cpp,
            )
        _LOGGER.info("Migration: Added include section to %s", main_cpp)

    write_file_if_changed(main_cpp, content)


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
    content = ""
    for key, value in sorted(data.items()):
        if isinstance(value, (list, set, tuple)):
            content += f"{key} =\n"
            for x in value:
                content += f"    {x}\n"
        else:
            content += f"{key} = {value}\n"
    return content


def gather_lib_deps():
    return [x.as_lib_dep for x in CORE.libraries]


def gather_build_flags():
    build_flags = CORE.build_flags

    # avoid changing build flags order
    return list(sorted(list(build_flags)))


ESP32_LARGE_PARTITIONS_CSV = """\
nvs,      data, nvs,     0x009000, 0x005000,
otadata,  data, ota,     0x00e000, 0x002000,
app0,     app,  ota_0,   0x010000, 0x1C0000,
app1,     app,  ota_1,   0x1D0000, 0x1C0000,
eeprom,   data, 0x99,    0x390000, 0x001000,
spiffs,   data, spiffs,  0x391000, 0x00F000
"""


def get_ini_content():
    lib_deps = gather_lib_deps()
    build_flags = gather_build_flags()

    data = {
        "platform": CORE.arduino_version,
        "board": CORE.board,
        "framework": "arduino",
        "lib_deps": lib_deps + ["${common.lib_deps}"],
        "build_flags": build_flags + ["${common.build_flags}"],
        "upload_speed": UPLOAD_SPEED_OVERRIDE.get(CORE.board, 115200),
    }

    if CORE.is_esp32:
        data["board_build.partitions"] = "partitions.csv"
        partitions_csv = CORE.relative_build_path("partitions.csv")
        write_file_if_changed(partitions_csv, ESP32_LARGE_PARTITIONS_CSV)

    # pylint: disable=unsubscriptable-object
    if CONF_BOARD_FLASH_MODE in CORE.config[CONF_ESPHOME]:
        flash_mode = CORE.config[CONF_ESPHOME][CONF_BOARD_FLASH_MODE]
        data["board_build.flash_mode"] = flash_mode

    # Build flags
    if CORE.is_esp8266 and CORE.board in ESP8266_FLASH_SIZES:
        flash_size = ESP8266_FLASH_SIZES[CORE.board]
        ld_scripts = ESP8266_LD_SCRIPTS[flash_size]

        versions_with_old_ldscripts = [
            ARDUINO_VERSION_ESP8266["2.4.0"],
            ARDUINO_VERSION_ESP8266["2.4.1"],
            ARDUINO_VERSION_ESP8266["2.4.2"],
        ]
        if CORE.arduino_version == ARDUINO_VERSION_ESP8266["2.3.0"]:
            # No ld script support
            ld_script = None
        if CORE.arduino_version in versions_with_old_ldscripts:
            # Old ld script path
            ld_script = ld_scripts[0]
        else:
            ld_script = ld_scripts[1]

        if ld_script is not None:
            data["board_build.ldscript"] = ld_script

    # Ignore libraries that are not explicitly used, but may
    # be added by LDF
    # data['lib_ldf_mode'] = 'chain'
    data.update(CORE.config[CONF_ESPHOME].get(CONF_PLATFORMIO_OPTIONS, {}))

    content = f"[env:{CORE.name}]\n"
    content += format_ini(data)

    return content


def find_begin_end(text, begin_s, end_s):
    begin_index = text.find(begin_s)
    if begin_index == -1:
        raise EsphomeError(
            "Could not find auto generated code begin in file, either "
            "delete the main sketch file or insert the comment again."
        )
    if text.find(begin_s, begin_index + 1) != -1:
        raise EsphomeError(
            "Found multiple auto generate code begins, don't know "
            "which to chose, please remove one of them."
        )
    end_index = text.find(end_s)
    if end_index == -1:
        raise EsphomeError(
            "Could not find auto generated code end in file, either "
            "delete the main sketch file or insert the comment again."
        )
    if text.find(end_s, end_index + 1) != -1:
        raise EsphomeError(
            "Found multiple auto generate code endings, don't know "
            "which to chose, please remove one of them."
        )

    return text[:begin_index], text[(end_index + len(end_s)) :]


def write_platformio_ini(content):
    update_storage_json()
    path = CORE.relative_build_path("platformio.ini")

    if os.path.isfile(path):
        text = read_file(path)
        content_format = find_begin_end(
            text, INI_AUTO_GENERATE_BEGIN, INI_AUTO_GENERATE_END
        )
    else:
        content_format = INI_BASE_FORMAT
    full_file = content_format[0] + INI_AUTO_GENERATE_BEGIN + "\n" + content
    full_file += INI_AUTO_GENERATE_END + content_format[1]
    write_file_if_changed(path, full_file)


def write_platformio_project():
    mkdir_p(CORE.build_path)

    content = get_ini_content()
    if not get_bool_env(ENV_NOGITIGNORE):
        write_gitignore()
    write_platformio_ini(content)


DEFINES_H_FORMAT = ESPHOME_H_FORMAT = """\
#pragma once
{}
"""
VERSION_H_FORMAT = """\
#pragma once
#define ESPHOME_VERSION "{}"
"""
DEFINES_H_TARGET = "esphome/core/defines.h"
VERSION_H_TARGET = "esphome/core/version.h"
ESPHOME_README_TXT = """
THIS DIRECTORY IS AUTO-GENERATED, DO NOT MODIFY

ESPHome automatically populates the esphome/ directory, and any
changes to this directory will be removed the next time esphome is
run.

For modifying esphome's core files, please use a development esphome install
or use the custom_components folder.
"""


def copy_src_tree():
    source_files: Dict[Path, loader.SourceFile] = {}
    for _, component, _ in iter_components(CORE.config):
        source_files.update(component.source_files)

    # Convert to list and sort
    source_files_l = list(source_files.items())
    source_files_l.sort()

    # Build #include list for esphome.h
    include_l = []
    for target, _ in source_files_l:
        if target.suffix in HEADER_FILE_EXTENSIONS:
            include_l.append(f'#include "{target}"')
    include_l.append("")
    include_s = "\n".join(include_l)

    source_files_copy = source_files.copy()
    ignore_targets = [Path(x) for x in (DEFINES_H_TARGET, VERSION_H_TARGET)]
    for t in ignore_targets:
        source_files_copy.pop(t)

    for fname in walk_files(CORE.relative_src_path("esphome")):
        p = Path(fname)
        if p.suffix not in SOURCE_FILE_EXTENSIONS:
            # Not a source file, ignore
            continue
        # Transform path to target path name
        target = p.relative_to(CORE.relative_src_path())
        if target in ignore_targets:
            # Ignore defines.h, will be dealt with later
            continue
        if target not in source_files_copy:
            # Source file removed, delete target
            p.unlink()
        else:
            src_file = source_files_copy.pop(target)
            with src_file.path() as src_path:
                copy_file_if_changed(src_path, p)

    # Now copy new files
    for target, src_file in source_files_copy.items():
        dst_path = CORE.relative_src_path(*target.parts)
        with src_file.path() as src_path:
            copy_file_if_changed(src_path, dst_path)

    # Finally copy defines
    write_file_if_changed(
        CORE.relative_src_path("esphome", "core", "defines.h"), generate_defines_h()
    )
    write_file_if_changed(
        CORE.relative_src_path("esphome", "README.txt"), ESPHOME_README_TXT
    )
    write_file_if_changed(
        CORE.relative_src_path("esphome.h"), ESPHOME_H_FORMAT.format(include_s)
    )
    write_file_if_changed(
        CORE.relative_src_path("esphome", "core", "version.h"),
        VERSION_H_FORMAT.format(__version__),
    )


def generate_defines_h():
    define_content_l = [x.as_macro for x in CORE.defines]
    define_content_l.sort()
    return DEFINES_H_FORMAT.format("\n".join(define_content_l))


def write_cpp(code_s):
    path = CORE.relative_src_path("main.cpp")
    if os.path.isfile(path):
        text = read_file(path)
        code_format = find_begin_end(
            text, CPP_AUTO_GENERATE_BEGIN, CPP_AUTO_GENERATE_END
        )
        code_format_ = find_begin_end(
            code_format[0], CPP_INCLUDE_BEGIN, CPP_INCLUDE_END
        )
        code_format = (code_format_[0], code_format_[1], code_format[1])
    else:
        code_format = CPP_BASE_FORMAT

    copy_src_tree()
    global_s = '#include "esphome.h"\n'
    global_s += CORE.cpp_global_section

    full_file = code_format[0] + CPP_INCLUDE_BEGIN + "\n" + global_s + CPP_INCLUDE_END
    full_file += (
        code_format[1] + CPP_AUTO_GENERATE_BEGIN + "\n" + code_s + CPP_AUTO_GENERATE_END
    )
    full_file += code_format[2]
    write_file_if_changed(path, full_file)


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
    path = CORE.relative_config_path(".gitignore")
    if not os.path.isfile(path):
        with open(path, "w") as f:
            f.write(GITIGNORE_CONTENT)

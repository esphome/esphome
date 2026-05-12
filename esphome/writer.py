import importlib
import json
import logging
import os
from pathlib import Path
import re
import time

from esphome import loader
from esphome.compiled_config import save_compiled_config
from esphome.config import iter_component_configs, iter_components
from esphome.const import (
    HEADER_FILE_EXTENSIONS,
    PLATFORM_ESP32,
    SOURCE_FILE_EXTENSIONS,
    __version__,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import (
    copy_file_if_changed,
    cpp_string_escape,
    is_ha_addon,
    read_file,
    rmtree,
    walk_files,
    write_file_if_changed,
)
from esphome.storage_json import StorageJSON, storage_path

_LOGGER = logging.getLogger(__name__)

CPP_AUTO_GENERATE_BEGIN = "// ========== AUTO GENERATED CODE BEGIN ==========="
CPP_AUTO_GENERATE_END = "// =========== AUTO GENERATED CODE END ============"
CPP_INCLUDE_BEGIN = "// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ==========="
CPP_INCLUDE_END = "// ========== AUTO GENERATED INCLUDE BLOCK END ==========="

CPP_BASE_FORMAT = (
    """// Auto generated code by esphome
""",
    """"

void setup() {
  """,
    """
  App.setup();
}

void loop() {
  App.loop();
}
""",
)

UPLOAD_SPEED_OVERRIDE = {
    "esp210": 57600,
}


def get_flags(key):
    flags = set()
    for _, component, conf in iter_component_configs(CORE.config):
        flags |= getattr(component, key)(conf)
    return flags


def get_include_text():
    include_text = '#include "esphome.h"\nusing namespace esphome;\n'
    for _, component, conf in iter_component_configs(CORE.config):
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
        include_text += f"{includes}\n"
    return include_text


def replace_file_content(text, pattern, repl):
    content_new, count = re.subn(pattern, repl, text, flags=re.MULTILINE)
    return content_new, count


def storage_should_clean(old: StorageJSON | None, new: StorageJSON) -> bool:
    if old is None:
        return True

    if old.src_version != new.src_version:
        return True
    if old.build_path != new.build_path:
        return True
    # Check if any components have been removed
    return bool(old.loaded_integrations - new.loaded_integrations)


def storage_should_update_cmake_cache(old: StorageJSON, new: StorageJSON) -> bool:
    # ESP32 uses CMake for both Arduino and ESP-IDF frameworks
    return (
        old.loaded_integrations != new.loaded_integrations
        or old.loaded_platforms != new.loaded_platforms
    ) and new.core_platform == PLATFORM_ESP32


def update_storage_json() -> None:
    path = storage_path()
    old = StorageJSON.load(path)
    new = StorageJSON.from_esphome_core(CORE, old)

    # Refresh the cache upload/logs read on the next call.
    if CORE.config is not None:
        save_compiled_config(CORE.config)

    if old == new:
        return

    if storage_should_clean(old, new):
        if old is not None and old.loaded_integrations - new.loaded_integrations:
            removed = old.loaded_integrations - new.loaded_integrations
            _LOGGER.info(
                "Components removed (%s), cleaning build files...",
                ", ".join(sorted(removed)),
            )
        else:
            _LOGGER.info("Core config or version changed, cleaning build files...")
        clean_build(clear_pio_cache=False)
    elif storage_should_update_cmake_cache(old, new):
        _LOGGER.info("Integrations changed, cleaning cmake cache...")
        clean_cmake_cache()

    new.save(path)


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


DEFINES_H_FORMAT = ESPHOME_H_FORMAT = """\
#pragma once
#include "esphome/core/macros.h"
{}
"""
VERSION_H_FORMAT = """\
#pragma once
#include "esphome/core/macros.h"
#define ESPHOME_VERSION "{}"
#define ESPHOME_VERSION_CODE VERSION_CODE({}, {}, {})
"""
DEFINES_H_TARGET = "esphome/core/defines.h"
VERSION_H_TARGET = "esphome/core/version.h"
BUILD_INFO_DATA_H_TARGET = "esphome/core/build_info_data.h"
BUILD_INFO_DATA_CPP_TARGET = "esphome/core/build_info_data.cpp"
ENTITY_TYPES_H_TARGET = "esphome/core/entity_types.h"
ESPHOME_README_TXT = """
THIS DIRECTORY IS AUTO-GENERATED, DO NOT MODIFY

ESPHome automatically populates the build directory, and any
changes to this directory will be removed the next time esphome is
run.

For modifying esphome's core files, please use a development esphome install,
the custom_components folder or the external_components feature.
"""


def copy_src_tree():
    source_files: list[loader.FileResource] = []
    for _, component in iter_components(CORE.config):
        source_files += component.resources
    source_files_map = {
        Path(x.package.replace(".", "/") + "/" + x.resource): x for x in source_files
    }

    # Convert to list and sort
    source_files_l = list(source_files_map.items())
    source_files_l.sort()

    # Build #include list for esphome.h
    # X-macro files are included multiple times with different macro definitions
    # and must not be included bare in esphome.h
    # Deprecated headers that re-export from a relocated component must not be
    # auto-included, since their #include of the new path only resolves when the
    # new component is loaded by a consumer.
    esphome_h_exclude = {
        Path(ENTITY_TYPES_H_TARGET),
        Path(
            "esphome/core/ring_buffer.h"
        ),  # moved to components/ring_buffer/, removed in 2026.11.0
    }
    include_l = []
    for target, _ in source_files_l:
        if target.suffix in HEADER_FILE_EXTENSIONS and target not in esphome_h_exclude:
            include_l.append(f'#include "{target}"')
    include_l.append("")
    include_s = "\n".join(include_l)

    source_files_copy = source_files_map.copy()
    ignore_targets = [
        Path(x)
        for x in (
            DEFINES_H_TARGET,
            VERSION_H_TARGET,
            BUILD_INFO_DATA_H_TARGET,
            BUILD_INFO_DATA_CPP_TARGET,
        )
    ]
    for t in ignore_targets:
        source_files_copy.pop(t, None)

    # Files to exclude from sources_changed tracking (generated files)
    generated_files = {
        Path("esphome/core/build_info_data.h"),
        Path("esphome/core/build_info_data.cpp"),
    }

    sources_changed = False
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
            if target not in generated_files:
                sources_changed = True
        else:
            src_file = source_files_copy.pop(target)
            with src_file.path() as src_path:
                if copy_file_if_changed(src_path, p) and target not in generated_files:
                    sources_changed = True

    # Now copy new files
    for target, src_file in source_files_copy.items():
        dst_path = CORE.relative_src_path(*target.parts)
        with src_file.path() as src_path:
            if (
                copy_file_if_changed(src_path, dst_path)
                and target not in generated_files
            ):
                sources_changed = True

    # Finally copy defines
    if write_file_if_changed(
        CORE.relative_src_path("esphome", "core", "defines.h"), generate_defines_h()
    ):
        sources_changed = True
    write_file_if_changed(CORE.relative_build_path("README.txt"), ESPHOME_README_TXT)
    if write_file_if_changed(
        CORE.relative_src_path("esphome.h"), ESPHOME_H_FORMAT.format(include_s)
    ):
        sources_changed = True
    if write_file_if_changed(
        CORE.relative_src_path("esphome", "core", "version.h"), generate_version_h()
    ):
        sources_changed = True

    # Generate new build_info files if needed
    build_info_data_h_path = CORE.relative_src_path(
        "esphome", "core", "build_info_data.h"
    )
    build_info_data_cpp_path = CORE.relative_src_path(
        "esphome", "core", "build_info_data.cpp"
    )
    build_info_json_path = CORE.relative_build_path("build_info.json")
    config_hash, build_time, build_time_str, comment = get_build_info()

    # Defensively force a rebuild if the build_info files don't exist, or if
    # there was a config change which didn't actually cause a source change
    if not build_info_data_h_path.exists() or not build_info_data_cpp_path.exists():
        sources_changed = True
    else:
        try:
            existing = json.loads(build_info_json_path.read_text(encoding="utf-8"))
            if (
                existing.get("config_hash") != config_hash
                or existing.get("esphome_version") != __version__
            ):
                sources_changed = True
        except (json.JSONDecodeError, KeyError, OSError):
            sources_changed = True

    # Write build_info header and JSON metadata
    if sources_changed:
        # write_file_if_changed avoids bumping mtime on identical content,
        # which is what makes the stable header actually isolate metadata churn.
        write_file_if_changed(
            build_info_data_h_path,
            generate_build_info_data_h(),
        )
        write_file_if_changed(
            build_info_data_cpp_path,
            generate_build_info_data_cpp(
                config_hash, build_time, build_time_str, comment
            ),
        )
        write_file_if_changed(
            build_info_json_path,
            json.dumps(
                {
                    "config_hash": config_hash,
                    "build_time": build_time,
                    "build_time_str": build_time_str,
                    "esphome_version": __version__,
                },
                indent=2,
            )
            + "\n",
        )

    platform = "esphome.components." + CORE.target_platform
    try:
        module = importlib.import_module(platform)
        copy_files = getattr(module, "copy_files")
        copy_files()
    except AttributeError:
        pass


def generate_defines_h():
    define_content_l = [x.as_macro for x in CORE.defines]
    define_content_l.sort()
    return DEFINES_H_FORMAT.format("\n".join(define_content_l))


def generate_version_h():
    match = re.match(r"^(\d+)\.(\d+).(\d+)-?\w*$", __version__)
    if not match:
        raise EsphomeError(f"Could not parse version {__version__}.")
    return VERSION_H_FORMAT.format(
        __version__, match.group(1), match.group(2), match.group(3)
    )


def get_build_info() -> tuple[int, int, str, str]:
    """Calculate build_info values from current config.

    Returns:
        Tuple of (config_hash, build_time, build_time_str, comment)
    """
    config_hash = CORE.config_hash
    build_time = int(time.time())
    build_time_str = time.strftime("%Y-%m-%d %H:%M:%S %z", time.localtime(build_time))
    comment = CORE.comment or ""
    return config_hash, build_time, build_time_str, comment


def generate_build_info_data_h() -> str:
    """Generate stable declarations for build info provided by generated C++."""
    return """#pragma once
// Auto-generated build_info declarations
#include <cstddef>
#include <cstdint>
#include <ctime>
#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome {
extern const uint32_t ESPHOME_CONFIG_HASH;
extern const time_t ESPHOME_BUILD_TIME;
extern const size_t ESPHOME_COMMENT_SIZE;
#ifdef USE_ESP8266
extern const char ESPHOME_BUILD_TIME_STR[] PROGMEM;
extern const char ESPHOME_COMMENT_STR[] PROGMEM;
#else
extern const char ESPHOME_BUILD_TIME_STR[];
extern const char ESPHOME_COMMENT_STR[];
#endif
}  // namespace esphome
"""


def generate_build_info_data_cpp(
    config_hash: int, build_time: int, build_time_str: str, comment: str
) -> str:
    """Generate build_info_data.cpp with config hash, build time, and comment."""
    from esphome.core.config import COMMENT_MAX_LEN

    # Defense-in-depth clamp; errors="ignore" drops a partial trailing UTF-8
    # sequence so the literal never decodes to a truncated codepoint.
    encoded = comment.encode("utf-8")[:COMMENT_MAX_LEN]
    comment = encoded.decode("utf-8", errors="ignore")
    # cpp_string_escape wraps in quotes; strip them since the template has them.
    escaped_comment = cpp_string_escape(comment)[1:-1]
    comment_size = len(comment.encode("utf-8")) + 1  # +1 for NUL
    return f"""// Auto-generated build_info data
#include "esphome/core/build_info_data.h"

namespace esphome {{
const uint32_t ESPHOME_CONFIG_HASH = 0x{config_hash:08x}U;  // NOLINT
const time_t ESPHOME_BUILD_TIME = {build_time};  // NOLINT
const size_t ESPHOME_COMMENT_SIZE = {comment_size};  // NOLINT
#ifdef USE_ESP8266
const char ESPHOME_BUILD_TIME_STR[] PROGMEM = "{build_time_str}";
const char ESPHOME_COMMENT_STR[] PROGMEM = "{escaped_comment}";
#else
const char ESPHOME_BUILD_TIME_STR[] = "{build_time_str}";
const char ESPHOME_COMMENT_STR[] = "{escaped_comment}";
#endif
}}  // namespace esphome
"""


def write_cpp(code_s):
    path = CORE.relative_src_path("main.cpp")
    if path.is_file():
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
    # using namespace esphome must precede all variable declarations since
    # codegen types assume this namespace is in scope (esphome_ns = global_ns).
    global_s = '#include "esphome.h"\n'
    global_s += "using namespace esphome;\n"
    global_s += CORE.cpp_global_section

    full_file = f"{code_format[0] + CPP_INCLUDE_BEGIN}\n{global_s}{CPP_INCLUDE_END}"
    full_file += (
        f"{code_format[1] + CPP_AUTO_GENERATE_BEGIN}\n{code_s}{CPP_AUTO_GENERATE_END}"
    )
    full_file += code_format[2]
    write_file_if_changed(path, full_file)


def clean_cmake_cache():
    pioenvs = CORE.relative_pioenvs_path()
    if pioenvs.is_dir():
        pioenvs_cmake_path = pioenvs / CORE.name / "CMakeCache.txt"
        if pioenvs_cmake_path.is_file():
            _LOGGER.info("Deleting %s", pioenvs_cmake_path)
            pioenvs_cmake_path.unlink()


def clean_build(clear_pio_cache: bool = True):
    # Allow skipping cache cleaning for integration tests
    if os.environ.get("ESPHOME_SKIP_CLEAN_BUILD"):
        _LOGGER.warning("Skipping build cleaning (ESPHOME_SKIP_CLEAN_BUILD set)")
        return

    pioenvs = CORE.relative_pioenvs_path()
    if pioenvs.is_dir():
        _LOGGER.info("Deleting %s", pioenvs)
        rmtree(pioenvs)
    piolibdeps = CORE.relative_piolibdeps_path()
    if piolibdeps.is_dir():
        _LOGGER.info("Deleting %s", piolibdeps)
        rmtree(piolibdeps)
    dependencies_lock = CORE.relative_build_path("dependencies.lock")
    if dependencies_lock.is_file():
        _LOGGER.info("Deleting %s", dependencies_lock)
        dependencies_lock.unlink()

    if not clear_pio_cache:
        return

    # Clean PlatformIO cache to resolve CMake compiler detection issues
    # This helps when toolchain paths change or get corrupted
    try:
        from platformio.project.config import ProjectConfig
    except ImportError:
        # PlatformIO is not available, skip cache cleaning
        pass
    else:
        config = ProjectConfig.get_instance()
        cache_dir = Path(config.get("platformio", "cache_dir"))
        if cache_dir.is_dir():
            _LOGGER.info("Deleting PlatformIO cache %s", cache_dir)
            rmtree(cache_dir)


def _get_custom_build_dir(item: Path, data_dir: Path) -> Path | None:
    """Parse a YAML config to find a custom build directory."""
    from esphome import yaml_util

    try:
        raw = yaml_util.load_yaml(item)
    except (EsphomeError, OSError) as e:
        _LOGGER.debug("Could not parse %s to find build_path: %s", item, e)
        return None
    if not isinstance(raw, dict):
        return None
    esphome_conf = raw.get("esphome", {})
    if not isinstance(esphome_conf, dict):
        return None
    if build_path := esphome_conf.get("build_path"):
        return data_dir / build_path
    return None


def clean_all(configuration: list[str]):
    data_dirs = []
    for config in configuration:
        item = Path(config)
        if item.is_file() and item.suffix in (".yaml", ".yml"):
            data_dir = item.parent / ".esphome"
            data_dirs.append(data_dir)
            if custom := _get_custom_build_dir(item, data_dir):
                data_dirs.append(custom)
        else:
            data_dirs.append(item / ".esphome")
    if is_ha_addon():
        data_dirs.append(Path("/data"))
    if env_data_dir := os.environ.get("ESPHOME_DATA_DIR"):
        data_dirs.append(Path(env_data_dir))
    if env_build_path := os.environ.get("ESPHOME_BUILD_PATH"):
        data_dirs.append(Path(env_build_path))
    if not data_dirs:
        # No config files or known data dirs, check current directory
        cwd_esphome = Path.cwd() / ".esphome"
        if cwd_esphome.is_dir():
            data_dirs.append(cwd_esphome)
        else:
            _LOGGER.warning(
                "No configuration files specified and no .esphome directory found in current directory. "
                "Pass YAML files or a configuration directory to clean build artifacts."
            )

    # Clean build dir
    for dir in data_dirs:
        if dir.is_dir():
            _LOGGER.info("Cleaning %s", dir)
            # Don't remove storage or .json files which are needed by the dashboard
            for item in dir.iterdir():
                if item.is_file() and not item.name.endswith(".json"):
                    item.unlink()
                elif item.is_dir() and item.name != "storage":
                    rmtree(item)

    # Clean PlatformIO project files
    try:
        from platformio.project.config import ProjectConfig
    except ImportError:
        # PlatformIO is not available, skip cleaning
        pass
    else:
        config = ProjectConfig.get_instance()
        for pio_dir in ["cache_dir", "packages_dir", "platforms_dir", "core_dir"]:
            path = Path(config.get("platformio", pio_dir))
            if path.is_dir():
                _LOGGER.info("Deleting PlatformIO %s %s", pio_dir, path)
                rmtree(path)


GITIGNORE_CONTENT = """# Gitignore settings for ESPHome
# This is an example and may include too much for your use-case.
# You can modify this file to suit your needs.
/.esphome/
/secrets.yaml
"""


def write_gitignore():
    path = CORE.relative_config_path(".gitignore")
    if not path.is_file():
        path.write_text(GITIGNORE_CONTENT, encoding="utf-8")

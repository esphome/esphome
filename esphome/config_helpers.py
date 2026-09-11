from collections.abc import Callable, Collection, Iterator
import logging
from pathlib import Path
import re

from esphome.const import (
    CONF_ESPHOME,
    CONF_INCLUDES,
    CONF_INCLUDES_C,
    CONF_LEVEL,
    CONF_LOGGER,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    SOURCE_FILE_EXTENSIONS,
    PlatformFramework,
)
from esphome.core import CORE, Lambda
from esphome.helpers import walk_files
from esphome.types import ConfigType
from esphome.util import OrderedDict

_LOGGER = logging.getLogger(__name__)

# Pre-build lookup map from (platform, framework) tuples to PlatformFramework enum
_PLATFORM_FRAMEWORK_LOOKUP = {
    (pf.value[0].value, pf.value[1].value): pf for pf in PlatformFramework
}


def merge_dicts_ordered(*dicts: dict) -> OrderedDict:
    """Merge multiple dicts into an OrderedDict, preserving key order.

    This is a helper to ensure that dictionary merging preserves OrderedDict type,
    which is important for operations like move_to_end().

    Args:
        *dicts: Variable number of dictionaries to merge (later dicts override earlier ones)

    Returns:
        OrderedDict with merged contents
    """
    result = OrderedDict()
    for d in dicts:
        if d:
            result.update(d)
    return result


class Extend:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"!extend {self.value}"

    def __repr__(self):
        return f"Extend({self.value})"

    def __eq__(self, b):
        """
        Check if two Extend objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Extend) and self.value == b.value


class Remove:
    def __init__(self, value=None):
        self.value = value

    def __str__(self):
        return f"!remove {self.value}"

    def __repr__(self):
        return f"Remove({self.value})"

    def __eq__(self, b):
        """
        Check if two Remove objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Remove) and self.value == b.value


def merge_config(old, new):
    if isinstance(new, Remove):
        return new
    if isinstance(new, dict):
        if not isinstance(old, dict):
            return new
        # Preserve OrderedDict type by copying to OrderedDict if either input is OrderedDict
        if isinstance(old, OrderedDict) or isinstance(new, OrderedDict):
            res = OrderedDict(old)
        else:
            res = old.copy()
        for k, v in new.items():
            res[k] = merge_config(old.get(k), v)
        return res
    if isinstance(new, list):
        if not isinstance(old, list):
            return new
        return old + new
    if new is None:
        return old

    return new


def frameworks_for_platforms(platforms: Collection[str]) -> set[PlatformFramework]:
    """All PlatformFramework members whose platform is in `platforms`.

    For FILTER_SOURCE_FILES maps that must stay in sync with a platform
    registry: deriving the framework set here means a platform added to the
    registry cannot validate and then fail at link on a filtered-out file.
    """
    known = {pf.value[0].value for pf in PlatformFramework}
    if unknown := set(platforms) - known:
        raise ValueError(f"unknown platform(s): {sorted(unknown)}")
    return {pf for pf in PlatformFramework if pf.value[0].value in platforms}


def filter_source_files_from_platform(
    files_map: dict[str, set[PlatformFramework]],
) -> Callable[[], list[str]]:
    """Helper to build a FILTER_SOURCE_FILES function from platform mapping.

    Args:
        files_map: Dict mapping filename to set of PlatformFramework enums
                  that should compile this file

    Returns:
        Function that returns list of files to exclude for current platform
    """

    def filter_source_files() -> list[str]:
        # Get current platform/framework
        core_data = CORE.data.get(KEY_CORE, {})
        target_platform = core_data.get(KEY_TARGET_PLATFORM)
        target_framework = core_data.get(KEY_TARGET_FRAMEWORK)

        if not target_platform or not target_framework:
            return []

        # Direct lookup of current PlatformFramework
        current_platform_framework = _PLATFORM_FRAMEWORK_LOOKUP.get(
            (target_platform, target_framework)
        )

        if not current_platform_framework:
            return []

        # Return files that should be excluded for current platform
        return [
            filename
            for filename, platforms in files_map.items()
            if current_platform_framework not in platforms
        ]

    return filter_source_files


def filter_source_files_from_defines(
    files_map: dict[str, str | tuple[str, ...]],
) -> Callable[[], list[str]]:
    """Helper to build a FILTER_SOURCE_FILES function from a define mapping.

    Args:
        files_map: Dict mapping filename to the define name (or tuple of
            define names) that keeps the file in the build; the file is
            excluded when none of its defines is set for the current config.

    Returns:
        Function that returns the files to exclude for the current config.
    """

    def filter_source_files() -> list[str]:
        defines = {define.name for define in CORE.defines}
        return [
            filename
            for filename, needed in files_map.items()
            if defines.isdisjoint((needed,) if isinstance(needed, str) else needed)
        ]

    return filter_source_files


def get_logger_level() -> str:
    """Get the configured logger level.

    This is used by components to determine what logging features to include
    based on the configured log level.

    Returns:
        The configured logger level string, defaults to "DEBUG" if not configured
    """
    # Check if logger config exists
    if CONF_LOGGER not in CORE.config:
        return "DEBUG"

    logger_config = CORE.config[CONF_LOGGER]
    return logger_config.get(CONF_LEVEL, "DEBUG")


# Heuristically matches scanf/sscanf calls with float format specifiers.
# Standard scanf float conversions: %f %F %e %E %g %G %a %A
# With optional modifiers: %*f (suppression), %8f (width), %lf %Lf (length)
# Also matches non-standard patterns like %.2f as a heuristic — these are
# invalid in scanf but users may write them by analogy with printf.
# Uses [^;]*? to stay within a single statement, preventing false positives
# from e.g. sscanf(buf, "%d", &x); printf("%f", val);
_SCANF_FLOAT_RE = re.compile(r"scanf\s*\([^;]*?%[*\d.]*[hlL]*[feEgGaAF]")


def lambdas_use_scanf_float(config: ConfigType) -> bool:
    """Check if any lambda in the config uses scanf with a float format specifier.

    Comments are stripped before matching to avoid false positives from
    commented-out code. The cost of a false positive is only the flash the
    caller's float scanf support takes.
    """
    stack: list = [config]
    while stack:
        obj = stack.pop()
        if isinstance(obj, Lambda):
            if "scanf" in obj.value and _SCANF_FLOAT_RE.search(
                obj.comment_remover(obj.value)
            ):
                return True
        elif isinstance(obj, dict):
            stack.extend(obj.values())
        elif isinstance(obj, list):
            stack.extend(obj)
    return False


def is_system_include(include: str) -> bool:
    """Whether an ``includes:`` entry is a ``<system>`` header rather than a local path."""
    return include.startswith("<") and include.endswith(">")


def iter_include_files(includes: list[str]) -> Iterator[tuple[Path, Path]]:
    """Yield ``(path, basename)`` for every local file named by ``includes:`` entries.

    A directory entry yields each file under it with a basename relative to the
    directory's parent, matching how it is copied into the build.
    """
    for include in includes:
        if is_system_include(include):
            continue
        path = CORE.relative_config_path(include)
        if path.is_dir():
            for file in walk_files(path):
                yield file, file.relative_to(path.parent)
        else:
            yield path, Path(path.name)


def includes_use_scanf_float(config: ConfigType) -> bool:
    """Check if an ``includes:`` or ``includes_c:`` file uses scanf with a float format specifier."""
    esphome_config = config.get(CONF_ESPHOME, {})
    includes = esphome_config.get(CONF_INCLUDES, []) + esphome_config.get(
        CONF_INCLUDES_C, []
    )
    for path, _ in iter_include_files(includes):
        if path.suffix not in SOURCE_FILE_EXTENSIONS:
            continue
        src = path.read_text(encoding="utf-8", errors="replace")
        if "scanf" in src and _SCANF_FLOAT_RE.search(Lambda.comment_remover(src)):
            return True
    return False


def user_code_uses_scanf_float(config: ConfigType) -> bool:
    """Whether a lambda or an ``includes:`` file scans a float."""
    return lambdas_use_scanf_float(config) or includes_use_scanf_float(config)


def keep_float_scanf(
    option: bool | None, config: ConfigType, flash_note: str, override_note: str
) -> bool:
    """Resolve a tri-state float-scanf option: unset means "only if user code scans a float".

    Logs when user code decides it, or (with ``override_note``, the consequence)
    when an explicit ``false`` overrides it.
    """
    if option is None:
        if not user_code_uses_scanf_float(config):
            return False
        _LOGGER.warning(
            "Lambda or include uses scanf with a float format specifier; "
            "keeping float scanf support (%s)",
            flash_note,
        )
        return True
    if not option and user_code_uses_scanf_float(config):
        _LOGGER.warning(
            "Float scanf support is disabled but a lambda or include uses scanf "
            "with a float format specifier; %s",
            override_note,
        )
    return option

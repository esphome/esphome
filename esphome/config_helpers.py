from collections.abc import Callable

from esphome.const import (
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PlatformFramework,
)
from esphome.core import CORE

# Pre-build lookup map from (platform, framework) tuples to PlatformFramework enum
_PLATFORM_FRAMEWORK_LOOKUP = {
    (pf.value[0].value, pf.value[1].value): pf for pf in PlatformFramework
}


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


def merge_config(full_old, full_new):
    def merge(old, new):
        if isinstance(new, dict):
            if not isinstance(old, dict):
                return new
            res = old.copy()
            for k, v in new.items():
                if isinstance(v, Remove) and k in old:
                    del res[k]
                else:
                    res[k] = merge(old[k], v) if k in old else v
            return res
        if isinstance(new, list):
            if not isinstance(old, list):
                return new
            res = old.copy()
            ids = {
                v_id: i
                for i, v in enumerate(res)
                if isinstance(v, dict)
                and (v_id := v.get(CONF_ID))
                and isinstance(v_id, str)
            }
            extend_ids = {
                v_id.value: i
                for i, v in enumerate(res)
                if isinstance(v, dict)
                and (v_id := v.get(CONF_ID))
                and isinstance(v_id, Extend)
            }

            ids_to_delete = []
            for v in new:
                if isinstance(v, dict) and (new_id := v.get(CONF_ID)):
                    if isinstance(new_id, Extend):
                        new_id = new_id.value
                        if new_id in ids:
                            v[CONF_ID] = new_id
                            res[ids[new_id]] = merge(res[ids[new_id]], v)
                            continue
                    elif isinstance(new_id, Remove):
                        new_id = new_id.value
                        if new_id in ids:
                            ids_to_delete.append(ids[new_id])
                            continue
                    elif (
                        new_id in extend_ids
                    ):  # When a package is extending a non-packaged item
                        extend_res = res[extend_ids[new_id]]
                        extend_res[CONF_ID] = new_id
                        new_v = merge(v, extend_res)
                        res[extend_ids[new_id]] = new_v
                        continue
                    else:
                        ids[new_id] = len(res)
                res.append(v)
            res = [v for i, v in enumerate(res) if i not in ids_to_delete]
            return res
        if new is None:
            return old

        return new

    return merge(full_old, full_new)


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

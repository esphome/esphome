from pathlib import Path

from esphome.build_helpers.tools_cache import ZEPHYR_TOOLS_CACHE, tools_cache_path


def get_zephyr_tools_path() -> Path:
    # Machine-global (OS user cache dir) so all Zephyr-based projects share one
    # SDK/toolchain/west install; see build_helpers.tools_cache.tools_cache_path
    # for the env-override and normalization rules.
    return tools_cache_path(*ZEPHYR_TOOLS_CACHE)

"""Machine-global tools cache location shared by the native backends."""

from __future__ import annotations

from pathlib import Path


def tools_cache_path(env_var: str, subdir: str) -> Path:
    """A backend's machine-global tools directory, with an env override.

    A blank/whitespace override is treated as unset: ``Path("")`` resolves
    to the CWD, which ``clean-all`` would then delete.
    """
    import platformdirs

    from esphome.helpers import get_str_env

    if prefix := get_str_env(env_var, "").strip():
        # resolve(): symlinked prefixes otherwise trip idf.py's
        # venv-mismatch warning on every build
        return Path(prefix).expanduser().resolve()
    # appauthor=False keeps the Windows path short (no vendor segment);
    # deep IDF trees run into MAX_PATH otherwise
    return (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / subdir
    ).resolve()


# (env override, cache subdir) per native backend. writer.clean_all wipes
# every entry via tools_cache_path, so listing a cache here is the single
# step that registers it for removal; the backends' own path getters use
# the same named pairs so the two cannot drift.
IDF_TOOLS_CACHE = ("ESPHOME_ESP_IDF_PREFIX", "idf")
SDK_NRF_TOOLS_CACHE = ("ESPHOME_SDK_NRF_PREFIX", "sdk-nrf")
ARDUINO8266_TOOLS_CACHE = ("ESPHOME_ARDUINO8266_PREFIX", "arduino8266")
TOOLS_CACHE_SPECS = (IDF_TOOLS_CACHE, SDK_NRF_TOOLS_CACHE, ARDUINO8266_TOOLS_CACHE)

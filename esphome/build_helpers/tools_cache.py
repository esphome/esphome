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
        return Path(prefix).expanduser().resolve()
    return (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / subdir
    ).resolve()

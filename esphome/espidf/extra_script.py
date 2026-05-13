"""Run a PlatformIO ``extraScript`` against a captured SCons-env stand-in.

PlatformIO libraries occasionally configure per-target link/build state
via a Python ``extraScript`` declared in ``library.json``'s ``build``
section instead of static fields. The script runs under SCons during
PIO's build and mutates the active ``Environment`` (``env.Append``,
``env.Replace``, …) — chiefly to set ``LIBPATH``/``LIBS`` per chip MCU.

ESPHome's PIO→IDF converter (``_generate_idf_component``) doesn't run
SCons, so these scripts were previously ignored and any library
relying on them failed to link under ``toolchain: esp-idf``. This
module provides a small shim that ``exec``s an extra-script with a
fake ``env`` object, captures the common ``env.Append(...)`` calls,
and returns the captured vars so the caller can fold them back into
the library's generated CMakeLists.

Caveats
-------
* Only the ``env.Append`` API is captured. ``env.Replace``,
  ``env.Prepend``, ``env.AddPreAction``, SCons file generators, and any
  arbitrary I/O are silently no-ops. Scripts that depend on those will
  produce incomplete output.
* Running arbitrary Python from third-party libraries is a non-trivial
  trust decision. The shim does no sandboxing — anything in the
  script's process can run. Use only with libraries whose source you
  trust.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
import os
from pathlib import Path

_LOGGER = logging.getLogger(__name__)

# Keys we know how to translate back into ESPHome's build-flag pipeline.
# Other env.Append kwargs are recorded but ignored downstream.
_CAPTURED_KEYS = frozenset({"LIBPATH", "LIBS", "CPPDEFINES", "LINKFLAGS", "CPPFLAGS"})


@dataclass
class ExtraScriptResult:
    """Build-var deltas captured from a PIO extra-script ``env.Append`` call."""

    libpath: list[str] = field(default_factory=list)
    libs: list[str] = field(default_factory=list)
    cppdefines: list[str | tuple[str, str]] = field(default_factory=list)
    linkflags: list[str] = field(default_factory=list)
    cppflags: list[str] = field(default_factory=list)


class _FakeSConsEnv:
    """Minimal stand-in for SCons ``Environment`` exposed to extra-scripts.

    Implements just enough surface area to let scripts query ``BOARD_MCU``
    / ``PIOENV`` and call ``env.Append(LIBPATH=…, LIBS=…, …)``. Every
    other env method swallows silently so unrelated calls don't raise
    ``AttributeError`` and abort the script.
    """

    def __init__(self, *, board_mcu: str, pio_env: str) -> None:
        self._vars: dict[str, str] = {
            "BOARD_MCU": board_mcu,
            "PIOPLATFORM": "espressif32",
            "PIOENV": pio_env,
        }
        self.result = ExtraScriptResult()

    # ----- SCons env API the common scripts use -----

    def get(self, key: str, default: str | None = None) -> str | None:
        return self._vars.get(key, default)

    def Append(self, **kwargs) -> None:  # noqa: N802 (SCons API name)
        for key, value in kwargs.items():
            if key not in _CAPTURED_KEYS:
                continue
            items = list(value) if isinstance(value, (list, tuple)) else [value]
            bucket = getattr(self.result, key.lower())
            bucket.extend(items)

    # ----- Everything else is a no-op so unsupported scripts don't crash -----

    def __getattr__(self, name: str):
        def _noop(*args, **kwargs):
            return None

        return _noop


def run_extra_script(
    script_path: Path, *, library_dir: Path, idf_target: str
) -> ExtraScriptResult:
    """Execute ``script_path`` with a fake SCons env and return captured vars.

    ``idf_target`` is the active ESP-IDF target name (e.g. ``esp32``,
    ``esp32s3``); it's exposed to the script as PlatformIO's
    ``BOARD_MCU`` so chip-conditional logic resolves the same way it
    would under PIO. The script runs with ``library_dir`` as the
    process CWD so relative-path lookups (``join``, ``realpath``,
    ``open``) resolve against the library tree.

    On any exception inside the script we log at debug level and return
    an empty result — extra-scripts are best-effort, and an unsupported
    script shouldn't block the build.
    """
    env = _FakeSConsEnv(board_mcu=idf_target, pio_env=f"esphome_{idf_target}")
    code = compile(script_path.read_text(), str(script_path), "exec")
    old_cwd = os.getcwd()
    try:
        os.chdir(library_dir)
        exec(  # noqa: S102 pylint: disable=exec-used
            code,
            {
                "Import": lambda *_args: None,  # SCons-side import; harmless here
                "env": env,
                "__file__": str(script_path),
                "__name__": "__pio_extra_script__",
            },
        )
    except Exception as e:  # pylint: disable=broad-exception-caught
        _LOGGER.warning("PIO extra-script %s raised %s; skipping", script_path, e)
        return ExtraScriptResult()
    finally:
        os.chdir(old_cwd)
    return env.result


def captured_as_build_flags(
    result: ExtraScriptResult, *, library_dir: Path
) -> list[str]:
    """Translate captured env vars into the ``-L`` / ``-l`` / ``-D`` /
    raw-flag form ``_generate_cmakelists_txt`` already knows how to consume.

    ``LIBPATH`` entries are made relative to ``library_dir`` so the
    generated CMakeLists is portable; absolute paths outside the library
    tree are kept as-is (CMake handles absolute paths in
    ``target_link_directories`` fine).
    """
    flags: list[str] = []
    library_root = library_dir.resolve()
    for path in result.libpath:
        # Anchor relative paths to library_dir (not the current CWD, which
        # has been restored by the time we get here). Joining an absolute
        # path against library_dir returns the absolute path unchanged.
        resolved = (library_dir / path).resolve()
        try:
            flags.append(f"-L{resolved.relative_to(library_root)}")
        except ValueError:
            flags.append(f"-L{resolved}")
    flags.extend(f"-l{lib}" for lib in result.libs)
    for define in result.cppdefines:
        if isinstance(define, tuple) and len(define) == 2:
            flags.append(f"-D{define[0]}={define[1]}")
        else:
            flags.append(f"-D{define}")
    flags.extend(result.linkflags)
    flags.extend(result.cppflags)
    return flags

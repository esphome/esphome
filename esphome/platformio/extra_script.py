"""Run a PlatformIO ``extraScript`` against a captured SCons-env stand-in.

PlatformIO libraries occasionally configure per-target link/build state
via a Python ``extraScript`` declared in ``library.json``'s ``build``
section instead of static fields. The script runs under SCons during
PIO's build and mutates the active ``Environment`` (``env.Append``,
``env.Replace``, …) — chiefly to set ``LIBPATH``/``LIBS`` per chip MCU.

ESPHome's PIO→IDF converter doesn't run SCons, so these scripts were
previously ignored and any library
relying on them failed to link under ``toolchain: esp-idf``. This
module provides a small shim that ``exec``s an extra-script with a
fake ``env`` object, captures the common ``env.Append(...)`` calls,
and returns the captured vars so the caller can fold them back into
the library's generated CMakeLists.

Caveats
-------
* Only the ``env.Append`` API is captured. ``env.Replace``,
  ``env.Prepend``, ``env.AddPreAction``, SCons file generators, and any
  arbitrary I/O are no-ops, logged once per method. Scripts that depend
  on those will produce incomplete output.
* Running arbitrary Python from third-party libraries is a non-trivial
  trust decision. The shim does no sandboxing — anything in the
  script's process can run. Use only with libraries whose source you
  trust.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
from typing import TYPE_CHECKING

from esphome.core import EsphomeError

if TYPE_CHECKING:
    from esphome.platformio.library import ConvertedLibrary

_LOGGER = logging.getLogger(__name__)


def apply_extra_script(
    component: ConvertedLibrary,
    board_mcu: Callable[[], str],
    pio_platform: str,
) -> None:
    """Run a library's PIO ``extraScript`` and fold its captured env vars into
    ``component.data["build"]["flags"]`` so the backend's -L/-l/-D extraction
    picks them up. Shared by the ESP-IDF and ESP8266 Arduino backends.

    ``board_mcu`` is a callable so a backend whose target lookup needs build
    state (the esp32 variant) resolves it only when a script will run.
    ``pio_platform`` is exposed to the script as PlatformIO's ``PIOPLATFORM``.
    """
    extra_script = component.data.get("build", {}).get("extraScript")
    if not extra_script:
        return
    # Resolve and confine to the library's source dir so a malicious
    # library.json can't escape (e.g. ``"extraScript": "../../etc/passwd"``).
    source_path = component.source_dir
    library_root = source_path.resolve()
    script_path = (source_path / extra_script).resolve()
    if not script_path.is_relative_to(library_root):
        # More hostile than a missing script; must not be quieter than it
        raise EsphomeError(
            f"extraScript {extra_script} of library {component.name} escapes "
            "the library directory"
        )
    if not script_path.is_file():
        # A declared-but-absent script is a broken or half-downloaded
        # package, not an unsupported script; PlatformIO fails on it too
        raise EsphomeError(
            f"extraScript {extra_script} of library {component.name} not found"
        )
    result = run_extra_script(
        script_path,
        library_dir=source_path,
        board_mcu=board_mcu(),
        pio_platform=pio_platform,
    )
    extra_flags = captured_as_build_flags(result, library_dir=source_path)
    if not extra_flags:
        return
    flags = component.data.setdefault("build", {}).setdefault("flags", [])
    if isinstance(flags, str):
        flags = [flags]
    elif not isinstance(flags, list):
        # A null/dict value coerced through a list wrapper would inject a
        # non-string into the compiler command line; fail naming the library
        raise EsphomeError(
            f"Library {component.name} has a malformed build.flags "
            f"({type(flags).__name__}); expected a string or list"
        )
    component.data["build"]["flags"] = [*flags, *extra_flags]


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

    def __init__(self, *, board_mcu: str, pio_env: str, pio_platform: str) -> None:
        self._vars: dict[str, str] = {
            "BOARD_MCU": board_mcu,
            "PIOPLATFORM": pio_platform,
            "PIOENV": pio_env,
        }
        self.result = ExtraScriptResult()
        self._warned_methods: set[str] = set()
        self._warned_keys: set[str] = set()

    # ----- SCons env API the common scripts use -----

    def get(self, key: str, default: str | None = None) -> str | None:
        return self._vars.get(key, default)

    def Append(self, **kwargs) -> None:  # noqa: N802 (SCons API name)
        for key, value in kwargs.items():
            if key not in _CAPTURED_KEYS:
                # Warn once per key so a loop of Appends cannot spam
                if key not in self._warned_keys:
                    self._warned_keys.add(key)
                    _LOGGER.warning(
                        "PIO extra-script env.Append(%s=...) is not captured; ignoring",
                        key,
                    )
                continue
            items = list(value) if isinstance(value, (list, tuple)) else [value]
            bucket = getattr(self.result, key.lower())
            bucket.extend(items)

    # ----- Everything else is a no-op so unsupported scripts don't crash -----

    def __getattr__(self, name: str):
        def _noop(*args, **kwargs):
            # Once per method: a script whose whole effect is env.Replace()
            # must be diagnosable from a normal build log
            if name not in self._warned_methods:
                self._warned_methods.add(name)
                _LOGGER.warning(
                    "PIO extra-script env.%s(...) is not supported; ignoring", name
                )

        return _noop


def run_extra_script(
    script_path: Path,
    *,
    library_dir: Path,
    board_mcu: str,
    pio_platform: str,
) -> ExtraScriptResult:
    """Execute ``script_path`` with a fake SCons env and return captured vars.

    ``board_mcu`` is the active MCU name (e.g. ``esp32``,
    ``esp32s3``); it's exposed to the script as PlatformIO's
    ``BOARD_MCU`` so chip-conditional logic resolves the same way it
    would under PIO. The script runs with ``library_dir`` as the
    process CWD so relative-path lookups (``join``, ``realpath``,
    ``open``) resolve against the library tree.

    On any exception inside the script we warn and return an empty result
    (never a partial capture, which could build wrong-output firmware) —
    extra-scripts are best-effort, and an unsupported script shouldn't
    block the build.
    """
    env = _FakeSConsEnv(
        board_mcu=board_mcu,
        pio_env=f"esphome_{board_mcu}",
        pio_platform=pio_platform,
    )
    try:
        source = script_path.read_text(encoding="utf-8")
    except OSError as err:
        # An unreadable declared script is a broken package, exactly like a
        # missing one; must not be quieter than that case
        raise EsphomeError(f"extraScript {script_path} is unreadable: {err}") from err
    except UnicodeDecodeError as e:
        # A content problem, best-effort like a SyntaxError below
        _LOGGER.warning(
            "PIO extra-script %s (in %s) is not UTF-8 (%r); ignoring its output",
            script_path,
            library_dir.name,
            e,
        )
        return ExtraScriptResult()
    old_cwd = Path.cwd()
    try:
        # Inside the try: a SyntaxError in a vendored script is just as
        # best-effort as a runtime failure
        code = compile(source, str(script_path), "exec")
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
    except SystemExit as e:
        if not e.code:
            # sys.exit() / sys.exit(0) is a normal PlatformIO script ending;
            # the capture is complete
            return env.result
        _LOGGER.warning(
            "PIO extra-script %s (in %s) exited with status %r; ignoring its output",
            script_path,
            library_dir.name,
            e.code,
        )
        return ExtraScriptResult()
    except Exception as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Discard any partial capture: half-applied flags could build wrong
        # firmware that links cleanly.
        _LOGGER.warning(
            "PIO extra-script %s (in %s) raised %r; ignoring its output",
            script_path,
            library_dir.name,
            e,
        )
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

"""Run a PlatformIO library ``extraScript`` against a fake SCons env.

The shim execs the script with a stand-in ``env``, captures ``env.Append``
calls (everything else is a logged no-op), and folds the result into the
library's build flags. No sandboxing: the script runs with full process
access, so it carries the same trust as the library's own source.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
import shlex
from typing import TYPE_CHECKING, Any, NamedTuple

from esphome.core import EsphomeError
from esphome.platformio.library import ESPHOME_DATA_KEY, ESPHOME_DATA_LINK_FLAGS_KEY

if TYPE_CHECKING:
    from esphome.platformio.library import ConvertedLibrary

_LOGGER = logging.getLogger(__name__)


def apply_extra_script(
    component: ConvertedLibrary,
    board_mcu: Callable[[], str],
    pio_platform: str,
) -> None:
    """Run a library's ``extraScript`` and fold its captured env vars into
    ``build.flags``; ``board_mcu`` is a callable so it resolves lazily."""
    extra_script = component.data.get("build", {}).get("extraScript")
    if extra_script is None or extra_script == "":
        return
    if not isinstance(extra_script, str):
        # A list/dict value would raise an opaque TypeError on the join below
        raise EsphomeError(
            f"extraScript of library {component.name} must be a string, "
            f"got {type(extra_script).__name__}"
        )
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
    if link_flags := _str_entries(result.linkflags, "LINKFLAGS"):
        # Kept apart from build.flags: the CMake emitters route those to
        # target_compile_options, where a link flag is silently ineffective
        esphome_data = component.data.setdefault(ESPHOME_DATA_KEY, {})
        esphome_data.setdefault(ESPHOME_DATA_LINK_FLAGS_KEY, []).extend(link_flags)
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
_CAPTURED_KEYS = frozenset(
    {"CPPPATH", "LIBPATH", "LIBS", "CPPDEFINES", "LINKFLAGS", "CPPFLAGS"}
)


@dataclass
class ExtraScriptResult:
    """Build-var deltas captured from a PIO extra-script ``env.Append`` call."""

    cpppath: list[str] = field(default_factory=list)
    libpath: list[str] = field(default_factory=list)
    libs: list[str] = field(default_factory=list)
    cppdefines: list[CppDefine] = field(default_factory=list)
    linkflags: list[str] = field(default_factory=list)
    cppflags: list[str] = field(default_factory=list)


class CppDefine(NamedTuple):
    """One normalized CPPDEFINES entry; a ``value`` of None is a bare -DNAME."""

    name: str
    value: str | None = None


def _cppdefine(entry: Any) -> CppDefine | None:
    """Normalize one CPPDEFINES element, or warn and drop an unsupported
    shape; formatting those blind would hand the compiler garbage like
    ``-D{'FOO': '1'}``."""
    if isinstance(entry, str):
        return CppDefine(entry)
    if (
        isinstance(entry, (tuple, list))
        and len(entry) == 2
        and isinstance(entry[0], (str, int))
        and isinstance(entry[1], (str, int, type(None)))
    ):
        value = entry[1]
        return CppDefine(str(entry[0]), None if value is None else str(value))
    _LOGGER.warning("Ignoring unsupported CPPDEFINES entry %r", entry)
    return None


def _cppdefines_items(value: Any) -> list[CppDefine]:
    """Normalize SCons ``processDefines`` spellings into ``CppDefine``s: a
    bare 2-tuple is one ``name=value`` pair, a dict maps names to values, a
    list is element-wise."""
    if isinstance(value, tuple) and len(value) == 2:
        elements: list[Any] = [value]
    elif isinstance(value, dict):
        elements = list(value.items())
    else:
        elements = list(value) if isinstance(value, (list, tuple)) else [value]
    return [d for e in elements if (d := _cppdefine(e)) is not None]


class _FakeSConsEnv:
    """Minimal SCons ``Environment`` stand-in: ``get`` and ``Append`` work;
    every other method is a swallowed no-op so scripts don't abort."""

    def __init__(self, *, board_mcu: str, pio_env: str, pio_platform: str) -> None:
        self._vars: dict[str, str] = {
            "BOARD_MCU": board_mcu,
            "PIOPLATFORM": pio_platform,
            "PIOENV": pio_env,
        }
        self.result = ExtraScriptResult()
        self._warned_methods: set[str] = set()
        self._warned_keys: set[str] = set()
        self._warned_gets: set[str] = set()

    # ----- SCons env API the common scripts use -----

    def get(self, key: str, default: str | None = None) -> str | None:
        if key not in self._vars and key not in self._warned_gets:
            # A script branching on an unmodelled var silently takes the
            # default branch; make that diagnosable from a normal build log
            self._warned_gets.add(key)
            _LOGGER.warning(
                "PIO extra-script env.get(%r) is not modelled; returning the default",
                key,
            )
        return self._vars.get(key, default)

    def __contains__(self, key: object) -> bool:
        # Without this, "KEY" in env falls back to the legacy sequence
        # protocol: __getitem__(0), (1), ... never raises, so it loops
        # forever flooding the log
        return key in self._vars

    def __iter__(self):
        return iter(self._vars)

    def __getitem__(self, key: str) -> str:
        # Scripts also read env["BOARD_MCU"]; an unmodelled subscript
        # degrades one branch instead of discarding the whole capture
        if key not in self._vars and key not in self._warned_gets:
            self._warned_gets.add(key)
            _LOGGER.warning(
                "PIO extra-script env[%r] is not modelled; returning ''", key
            )
        return self._vars.get(key, "")

    def Append(self, **kwargs) -> None:  # noqa: N802 (SCons API name)
        self._add(kwargs, prepend=False)

    def Prepend(self, **kwargs) -> None:  # noqa: N802 (SCons API name)
        self._add(kwargs, prepend=True)

    def _add(self, kwargs: dict[str, Any], *, prepend: bool) -> None:
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
            if key == "CPPDEFINES":
                items = _cppdefines_items(value)
            else:
                items = list(value) if isinstance(value, (list, tuple)) else [value]
            bucket = getattr(self.result, key.lower())
            if prepend:
                # SCons order: new values ahead of what is already there
                # (scripts prepend LIBS for static-link symbol resolution)
                bucket[:0] = items
            else:
                bucket.extend(items)

    # Dedup is not modelled; a repeated flag is harmless on the command line
    AppendUnique = Append
    PrependUnique = Prepend

    # ----- Everything else is a no-op so unsupported scripts don't crash -----

    def __getattr__(self, name: str):
        if name.startswith("__") and name.endswith("__"):
            # Protocol probes (copy, pickle, iteration) are not script calls
            raise AttributeError(name)
        if name not in self._warned_methods:
            # Warn on access, not call: hasattr()/truthiness branches would
            # otherwise silently take the wrong path; a script whose whole
            # effect is env.Replace() stays diagnosable either way
            self._warned_methods.add(name)
            _LOGGER.warning("PIO extra-script env.%s is not supported; ignoring", name)

        def _noop(*args, **kwargs):
            return None

        return _noop


def run_extra_script(
    script_path: Path,
    *,
    library_dir: Path,
    board_mcu: str,
    pio_platform: str,
) -> ExtraScriptResult:
    """Execute ``script_path`` with a fake SCons env, ``library_dir`` as CWD.

    A crashed script warns and returns an empty result, never a partial
    capture."""
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


def _str_entries(bucket: list, kind: str) -> list[str]:
    # Third-party scripts legally append SCons nodes, ints, or dicts;
    # stringifying those into flags would hand the compiler garbage
    good = [entry for entry in bucket if isinstance(entry, str)]
    for entry in bucket:
        if not isinstance(entry, str):
            _LOGGER.warning("Ignoring unsupported %s entry %r", kind, entry)
    return good


def captured_as_build_flags(
    result: ExtraScriptResult, *, library_dir: Path
) -> list[str]:
    """Translate captured env vars into -L/-l/-D/raw build flags; path
    entries anchor to ``library_dir`` so the build files stay portable."""
    flags: list[str] = []
    library_root = library_dir.resolve()

    def _anchored(path: str) -> str:
        # Anchor relative paths to library_dir; the script's CWD has been
        # restored by now
        resolved = (library_dir / path).resolve()
        try:
            return str(resolved.relative_to(library_root))
        except ValueError:
            return str(resolved)

    # shlex.quote so a spaced path survives lex_build_flags as one token
    flags.extend(
        f"-I{shlex.quote(_anchored(path))}"
        for path in _str_entries(result.cpppath, "CPPPATH")
    )
    flags.extend(
        f"-L{shlex.quote(_anchored(path))}"
        for path in _str_entries(result.libpath, "LIBPATH")
    )
    flags.extend(f"-l{shlex.quote(lib)}" for lib in _str_entries(result.libs, "LIBS"))
    for define in result.cppdefines:
        if define.value is None:
            # {"FOO": None} / ("FOO", None) is a bare -DFOO in SCons
            flags.append(shlex.quote(f"-D{define.name}"))
        else:
            flags.append(shlex.quote(f"-D{define.name}={define.value}"))
    # Each captured entry is one argv token in SCons; quote so the
    # lex_build_flags round-trip cannot split a spaced value into two.
    # LINKFLAGS are deliberately absent: they travel via
    # ESPHOME_DATA_LINK_FLAGS_KEY straight to the link line.
    flags.extend(shlex.quote(f) for f in _str_entries(result.cppflags, "CPPFLAGS"))
    return flags

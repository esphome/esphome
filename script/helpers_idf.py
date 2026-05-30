"""Build clang-tidy idedata from an ESP-IDF native-toolchain build.

The PlatformIO toolchain exposes a curated ``pio run -t idedata`` JSON. The
native ESP-IDF toolchain has no such command -- instead its CMake build emits
``build/compile_commands.json`` (CMAKE_EXPORT_COMPILE_COMMANDS). This module
derives the same idedata fields clang-tidy needs from that file:

    {cxx_path, cxx_flags, defines, includes: {build, toolchain}}

This mirrors ``helpers_zephyr.py``, which already does the equivalent for the
Zephyr toolchain.
"""

import json
from pathlib import Path
import shlex
import subprocess

# GCC response files (``@file``) embed flags that must be expanded so GCC-only
# flags inside them go through the same filtering clang-tidy applies; left as
# ``@file`` clang would read them and choke on e.g. ``-mlongcalls``.


def _expand_response_files(tokens: list[str], directory: Path) -> list[str]:
    """Inline any ``@response-file`` arguments (paths relative to ``directory``)."""
    out: list[str] = []
    for tok in tokens:
        if tok.startswith("@"):
            rf = Path(tok[1:])
            if not rf.is_absolute():
                rf = directory / rf
            try:
                out.extend(
                    _expand_response_files(shlex.split(rf.read_text()), directory)
                )
                continue
            except OSError:
                pass  # fall through and keep the literal token
        out.append(tok)
    return out


def _pick_entry(entries: list[dict]) -> dict:
    """Pick a representative ESPHome C++ translation unit.

    All ESPHome sources share the same component include set / flags, so any
    one of them yields the include dirs + defines clang-tidy needs.
    """
    for entry in entries:
        f = entry["file"]
        if "/src/esphome/" in f and f.endswith((".cpp", ".cc")):
            return entry
    # Fallback: any C++ translation unit
    for entry in entries:
        if entry["file"].endswith((".cpp", ".cc")):
            return entry
    raise ValueError("no C++ translation unit found in compile_commands.json")


def _get_toolchain_includes(cxx_path: str) -> list[str]:
    """Query the compiler for its builtin ``#include <...>`` search dirs."""
    result = subprocess.run(
        [cxx_path, "-E", "-x", "c++", "-", "-v"],
        input="",
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
    )
    includes: list[str] = []
    capture = False
    for line in result.stderr.splitlines():
        if "#include <...> search starts here:" in line:
            capture = True
            continue
        if "End of search list." in line:
            break
        if capture:
            includes.append(line.strip())
    return includes


def _parse_entry(entry: dict) -> tuple[str, list[str], list[str], list[str]]:
    """Parse one compile_commands entry -> (cxx_path, defines, includes, cxx_flags)."""
    directory = Path(entry["directory"])
    tokens = _expand_response_files(shlex.split(entry["command"]), directory)

    cxx_path = tokens[0]
    defines: list[str] = []
    includes: list[str] = []
    cxx_flags: list[str] = []

    it = iter(tokens[1:])
    for tok in it:
        if tok in ("-c", "-o"):
            next(it, None)  # drop the flag and its argument (input/output)
        elif tok.startswith("-D"):
            defines.append(tok[2:] if len(tok) > 2 else next(it, ""))
        elif tok.startswith("-I"):
            includes.append(tok[2:] if len(tok) > 2 else next(it, ""))
        elif tok == "-isystem":
            includes.append(next(it, ""))
        elif tok.startswith("-isystem"):
            includes.append(tok[len("-isystem") :])
        elif tok in ("-MT", "-MF", "-MQ"):
            next(it, None)  # dependency-file flag + its argument
        elif tok.startswith(("-MD", "-MMD", "-MP", "-MM")):
            pass  # dependency-generation flags, no argument
        elif tok.endswith((".cpp", ".cc", ".c", ".o", ".S", ".s")):
            pass  # input/output files
        else:
            cxx_flags.append(tok)
    return cxx_path, defines, includes, cxx_flags


def idedata_from_compile_commands(compile_commands: Path) -> dict:
    """Parse compile_commands.json into the idedata fields clang-tidy consumes.

    A single ESP-IDF compile entry only carries its own component's REQUIRES
    include set, but clang-tidy analyzes ESPHome headers that transitively pull
    in other components (e.g. ``ArduinoJson.h`` via the json component). So take
    cxx_path / cxx_flags / defines from a representative ESPHome TU, but union
    the include dirs across all ESPHome TUs to get a project-wide superset
    (matching what PlatformIO's idedata provides).
    """
    entries = json.loads(Path(compile_commands).read_text(encoding="utf-8"))
    representative = _pick_entry(entries)
    cxx_path, defines, _, cxx_flags = _parse_entry(representative)

    # Union include dirs across all ESPHome translation units (order-preserving).
    build_includes: dict[str, None] = {}
    for entry in entries:
        if "/src/esphome/" not in entry["file"] or not entry["file"].endswith(
            (".cpp", ".cc")
        ):
            continue
        for inc in _parse_entry(entry)[2]:
            build_includes.setdefault(inc, None)

    return {
        "cxx_path": cxx_path,
        "cxx_flags": cxx_flags,
        "defines": defines,
        "includes": {
            "build": list(build_includes),
            "toolchain": _get_toolchain_includes(cxx_path),
        },
    }

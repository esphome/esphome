"""Tests for esphome.build_helpers.idedata (compile_commands.json -> idedata)."""

# pylint: disable=protected-access

import json
import logging
import os
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_helpers import idedata
from esphome.core import EsphomeError

# An absolute, forward-slash (shlex-safe) path prefix valid on the host OS, so
# tests exercise the same is-absolute / normalize behavior as a real compile DB
# (a drive-qualified path on Windows, a leading slash elsewhere).
ABS = "C:/" if os.name == "nt" else "/"


def _entry(directory: str, file: str, command: str) -> dict:
    return {"directory": directory, "file": file, "command": command}


def test_parse_entry_extracts_fields() -> None:
    """cxx_path, defines, includes and remaining flags are split apart."""
    entry = _entry(
        f"{ABS}build",
        f"{ABS}build/src/esphome/core/application.cpp",
        f"/tools/xtensa-esp32-elf-g++ -DUSE_ESP32 -DESPHOME_LOG_LEVEL=5 "
        f"-I{ABS}inc/a -isystem {ABS}sys/b -std=gnu++20 -c app.cpp -o app.cpp.o",
    )

    cxx_path, defines, includes, cxx_flags = idedata.parse_entry(entry)

    assert cxx_path == "/tools/xtensa-esp32-elf-g++"
    assert "USE_ESP32" in defines
    assert "ESPHOME_LOG_LEVEL=5" in defines
    assert f"{ABS}inc/a" in includes
    assert f"{ABS}sys/b" in includes
    assert "-std=gnu++20" in cxx_flags
    # input/output files and their flags are not treated as flags
    assert "-c" not in cxx_flags
    assert "-o" not in cxx_flags
    assert "app.cpp" not in cxx_flags
    assert "app.cpp.o" not in cxx_flags


def test_parse_entry_space_separated_args() -> None:
    """``-D X`` / ``-I path`` (separate arg) and ``-isystem<path>`` (joined)."""
    entry = _entry(
        f"{ABS}build",
        f"{ABS}build/src/esphome/x.cpp",
        f"g++ -D FOO=1 -I {ABS}inc/sep -isystem{ABS}sys/joined -c x.cpp",
    )

    _, defines, includes, _ = idedata.parse_entry(entry)

    assert "FOO=1" in defines
    assert f"{ABS}inc/sep" in includes
    assert f"{ABS}sys/joined" in includes


def test_parse_entry_resolves_relative_includes() -> None:
    """Relative includes are resolved against the entry's ``directory``."""
    directory = f"{ABS}build/proj"
    entry = _entry(
        directory,
        f"{directory}/src/esphome/x.cpp",
        "g++ -Iconfig -I../shared -isystem rel/sys -c x.cpp",
    )

    _, _, includes, _ = idedata.parse_entry(entry)

    def resolved(rel: str) -> str:
        # parse_entry emits forward slashes for consistency (normpath would
        # yield backslashes on Windows).
        return os.path.normpath(Path(directory) / rel).replace("\\", "/")

    assert resolved("config") in includes
    assert resolved("../shared") in includes  # ../ normalized away
    assert resolved("rel/sys") in includes
    # nothing is left relative
    assert all(Path(inc).is_absolute() for inc in includes)


def test_parse_entry_skips_dependency_flags() -> None:
    """Dependency-generation flags (and their args) are dropped."""
    entry = _entry(
        "/build",
        "/build/src/esphome/x.cpp",
        "g++ -MD -MT x.cpp.o -MF x.cpp.o.d -c x.cpp -o x.cpp.o",
    )

    _, _, _, cxx_flags = idedata.parse_entry(entry)

    for tok in ("-MD", "-MT", "x.cpp.o", "-MF", "x.cpp.o.d", "-c", "-o", "x.cpp"):
        assert tok not in cxx_flags


def test_expand_response_files(tmp_path: Path) -> None:
    """``@file`` arguments are inlined relative to the directory."""
    rsp = tmp_path / "flags.rsp"
    rsp.write_text("-DFROM_RSP -I/rsp/inc")

    tokens = idedata._expand_response_files(
        ["g++", f"@{rsp.name}", "-c", "x.cpp"], tmp_path
    )

    assert "-DFROM_RSP" in tokens
    assert "-I/rsp/inc" in tokens
    assert not any(t.startswith("@") for t in tokens)


def test_expand_response_files_keeps_literal_when_missing(tmp_path: Path) -> None:
    """An unreadable ``@file`` token is kept verbatim rather than dropped."""
    tokens = idedata._expand_response_files(["g++", "@nope.rsp"], tmp_path)
    assert "@nope.rsp" in tokens


def test_pick_entry_prefers_esphome_tu() -> None:
    """A ``/src/esphome/`` C++ TU is picked over other compile entries."""
    entries = [
        _entry("/b", "/b/managed_components/foo/foo.c", "gcc -c foo.c"),
        _entry("/b", "/b/src/esphome/core/app.cpp", "g++ -c app.cpp"),
    ]
    assert idedata._pick_entry(entries)["file"].endswith("app.cpp")


def test_pick_entry_falls_back_to_any_cxx_tu() -> None:
    """With no ``/src/esphome/`` TU present, the first C++ entry is the fallback."""
    entries = [
        _entry("/b", "/b/managed_components/foo/foo.c", "gcc -c foo.c"),
        _entry("/b", "/b/components/x/x.cpp", "g++ -c x.cpp"),
    ]
    assert idedata._pick_entry(entries)["file"].endswith("x.cpp")


def test_is_esphome_src_handles_backslash_paths() -> None:
    r"""The src marker must match Windows ``\src\esphome\`` paths too.

    compile_commands ``file`` entries use the OS-native separator; if the
    marker only matched forward slashes no source would match on Windows and
    the build-include union would be silently empty.
    """
    assert idedata._is_esphome_src(r"C:\b\src\esphome\core\app.cpp")
    assert idedata._is_esphome_src("/b/src/esphome/core/app.cpp")
    # non-esphome and non-C++ still rejected regardless of separator
    assert not idedata._is_esphome_src(r"C:\b\managed_components\x\x.cpp")
    assert not idedata._is_esphome_src(r"C:\b\src\esphome\core\app.h")


def test_idedata_from_build(tmp_path: Path) -> None:
    """Full transform: representative entry + include union + toolchain dirs."""
    compile_commands = tmp_path / "compile_commands.json"
    entries = [
        _entry(
            f"{ABS}b",
            f"{ABS}b/src/esphome/core/app.cpp",
            f"g++ -DUSE_ESP32 -I{ABS}inc/core -std=gnu++20 -c app.cpp -o app.cpp.o",
        ),
        _entry(
            f"{ABS}b",
            f"{ABS}b/src/esphome/sensor/s.cpp",
            f"g++ -DUSE_ESP32 -I{ABS}inc/sensor -c s.cpp -o s.cpp.o",
        ),
        # non-esphome TU: its includes must not leak into the union
        _entry(
            f"{ABS}b",
            f"{ABS}b/managed_components/x/x.c",
            f"gcc -I{ABS}inc/managed -c x.c",
        ),
    ]
    compile_commands.write_text(json.dumps(entries))

    fake_proc = MagicMock(
        returncode=0,
        stderr=(
            "ignored\n"
            "#include <...> search starts here:\n"
            " /tc/inc/c++\n"
            " /tc/inc\n"
            "End of search list.\n"
            "more ignored\n"
        ),
    )
    with patch.object(idedata.subprocess, "run", return_value=fake_proc):
        data = idedata.idedata_from_build(compile_commands)

    assert data["cxx_path"] == "g++"
    assert "USE_ESP32" in data["defines"]
    assert "-std=gnu++20" in data["cxx_flags"]
    # include dirs unioned across all esphome TUs
    assert f"{ABS}inc/core" in data["includes"]["build"]
    assert f"{ABS}inc/sensor" in data["includes"]["build"]
    # the non-esphome TU is excluded from the union
    assert f"{ABS}inc/managed" not in data["includes"]["build"]
    # toolchain search dirs parsed from the compiler's -v output
    assert data["includes"]["toolchain"] == ["/tc/inc/c++", "/tc/inc"]


def test_get_toolchain_includes_raises_on_probe_failure() -> None:
    """A failed compiler probe is a hard error, not a silent empty list."""
    fake_proc = MagicMock(returncode=1, stderr="xtensa-esp32-elf-g++: not found")
    with (
        patch.object(idedata.subprocess, "run", return_value=fake_proc),
        pytest.raises(RuntimeError, match="builtin include dirs"),
    ):
        idedata.get_toolchain_includes("/bad/compiler")


def test_get_toolchain_includes_raises_when_no_dirs_found() -> None:
    """Markers present but no dirs (anomalous output) also raises."""
    fake_proc = MagicMock(
        returncode=0,
        stderr="#include <...> search starts here:\nEnd of search list.\n",
    )
    with (
        patch.object(idedata.subprocess, "run", return_value=fake_proc),
        pytest.raises(RuntimeError, match="builtin include dirs"),
    ):
        idedata.get_toolchain_includes("/some/compiler")


# ESP-IDF's compile_commands.json on Windows mixes literal backslash path
# separators in the compiler path with shell ``\"`` quote-escaping in defines,
# which only the real Windows argv parser handles. These exercise that path.
@pytest.mark.skipif(os.name != "nt", reason="Windows argv tokenization")
def test_split_command_preserves_paths_and_unescapes_quotes() -> None:
    r"""Backslash paths survive while ``\"`` define-quoting is unescaped."""
    command = r"C:\esp\bin\riscv32-esp-elf-g++.exe -DVER=\"1.2.3\" -IC:/inc/a -c x.cpp"

    tokens = idedata._split_command(command)

    assert tokens[0] == r"C:\esp\bin\riscv32-esp-elf-g++.exe"
    assert '-DVER="1.2.3"' in tokens
    assert "-IC:/inc/a" in tokens


@pytest.mark.skipif(os.name != "nt", reason="Windows argv tokenization")
def test_split_command_empty_returns_empty() -> None:
    """An empty or blank command tokenizes to ``[]`` (e.g. an empty response file).

    Guards against ``CommandLineToArgvW("")`` returning the current process name
    instead of an empty list.
    """
    assert idedata._split_command("") == []
    assert idedata._split_command("   ") == []


@pytest.mark.skipif(os.name != "nt", reason="Windows argv tokenization")
def test_parse_entry_normalizes_windows_cxx_path() -> None:
    """A backslash compiler path is emitted forward-slashed; define unescaped."""
    entry = _entry(
        r"C:\b",
        r"C:\b\src\esphome\x.cpp",
        r"C:\esp\bin\g++.exe -DVER=\"1.2.3\" -IC:/inc/a -c x.cpp",
    )

    cxx_path, defines, includes, _ = idedata.parse_entry(entry)

    assert cxx_path == "C:/esp/bin/g++.exe"
    assert "\\" not in cxx_path
    assert 'VER="1.2.3"' in defines
    assert "C:/inc/a" in includes


def test_parse_entry_strips_launcher_prefix() -> None:
    """A launcher-wrapped compile names the compiler second; the exact
    configured launcher is stripped, not anything ccache-shaped."""
    entry = _entry(
        f"{ABS}build",
        f"{ABS}build/src/esphome/core/application.cpp",
        "/opt/homebrew/bin/ccache /tools/xtensa-lx106-elf-g++ -DUSE_ESP8266 "
        "-c app.cpp -o app.cpp.o",
    )
    cxx_path, defines, _, _ = idedata.parse_entry(
        entry, launcher="/opt/homebrew/bin/ccache"
    )
    assert cxx_path == "/tools/xtensa-lx106-elf-g++"
    assert defines == ["USE_ESP8266"]


def test_parse_entry_recovers_from_unconfigured_launcher(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A stale compile DB built with a launcher this run no longer configures
    still yields the real compiler (the next token), not the launcher."""
    entry = _entry(
        f"{ABS}build",
        f"{ABS}build/src/esphome/core/application.cpp",
        "/opt/homebrew/bin/ccache /tools/xtensa-lx106-elf-g++ -c a.cpp -o a.o",
    )
    caplog.set_level(logging.DEBUG)
    cxx_path, _, _, _ = idedata.parse_entry(entry)
    assert cxx_path == "/tools/xtensa-lx106-elf-g++"
    assert "Stripping unconfigured launcher" in caplog.text


def test_parse_entry_keeps_launcher_without_program() -> None:
    """A launcher followed only by flags (no program to recover) stays as
    token zero; the cache layer refuses to persist it."""
    entry = _entry(
        f"{ABS}build",
        f"{ABS}build/src/esphome/core/application.cpp",
        "/opt/homebrew/bin/ccache -c a.cpp -o a.o",
    )
    cxx_path, _, _, _ = idedata.parse_entry(entry)
    assert cxx_path == "/opt/homebrew/bin/ccache"


def _write_compile_commands(tmp_path: Path) -> Path:
    compile_commands = tmp_path / "compile_commands.json"
    compile_commands.write_text(
        json.dumps(
            [
                _entry(
                    f"{ABS}build",
                    f"{ABS}build/src/esphome/core/application.cpp",
                    "/tools/g++ -DUSE_ESP8266 -c app.cpp -o app.cpp.o",
                )
            ]
        )
    )
    return compile_commands


def test_load_or_build_idedata_missing_compile_db(tmp_path: Path) -> None:
    assert (
        idedata.load_or_build_idedata(
            tmp_path / "compile_commands.json", tmp_path / "f.elf", tmp_path / "c.json"
        )
        is None
    )


def test_load_or_build_idedata_builds_and_caches(tmp_path: Path) -> None:
    compile_commands = _write_compile_commands(tmp_path)
    cache = tmp_path / "cache" / "test.json"
    with patch.object(
        idedata, "get_toolchain_includes", return_value=["/toolchain/include"]
    ):
        data = idedata.load_or_build_idedata(
            compile_commands, tmp_path / "firmware.elf", cache
        )
    assert data["cc_path"] == "/tools/gcc"
    assert data["prog_path"] == str(tmp_path / "firmware.elf")
    assert json.loads(cache.read_text()) == data

    # A fresh cache is served without re-parsing the compile DB
    os.utime(cache, (compile_commands.stat().st_mtime + 10,) * 2)
    with patch.object(idedata, "idedata_from_build") as mock_build:
        assert (
            idedata.load_or_build_idedata(
                compile_commands, tmp_path / "firmware.elf", cache
            )
            == data
        )
    mock_build.assert_not_called()


def test_load_or_build_idedata_rebuilds_bad_cache(tmp_path: Path) -> None:
    compile_commands = _write_compile_commands(tmp_path)
    cache = tmp_path / "cache.json"
    for bad in ("not json", json.dumps({"no_cc_path": True})):
        cache.write_text(bad)
        os.utime(cache, (compile_commands.stat().st_mtime + 10,) * 2)
        with patch.object(idedata, "get_toolchain_includes", return_value=[]):
            data = idedata.load_or_build_idedata(
                compile_commands, tmp_path / "f.elf", cache
            )
        assert "cc_path" in data


def test_load_or_build_idedata_rebuilds_when_compile_db_newer(tmp_path: Path) -> None:
    """A compile DB newer than the cache forces regeneration."""
    compile_commands = _write_compile_commands(tmp_path)
    cache = tmp_path / "cache.json"
    cache.write_text(json.dumps({"cc_path": "stale"}))
    os.utime(compile_commands, (cache.stat().st_mtime + 10,) * 2)
    with patch.object(idedata, "get_toolchain_includes", return_value=[]):
        data = idedata.load_or_build_idedata(
            compile_commands, tmp_path / "f.elf", cache
        )
    assert data["cc_path"] != "stale"


def test_load_or_build_idedata_rebuilds_non_dict_cache(tmp_path: Path) -> None:
    """Valid JSON that is not an object is regenerated, never handed out.

    A bare string would otherwise pass the cc_path check by substring.
    """
    compile_commands = _write_compile_commands(tmp_path)
    cache = tmp_path / "cache.json"
    for bad in ('"cc_path is a string"', "[]", "42"):
        cache.write_text(bad)
        os.utime(cache, (compile_commands.stat().st_mtime + 10,) * 2)
        with patch.object(idedata, "get_toolchain_includes", return_value=[]):
            data = idedata.load_or_build_idedata(
                compile_commands, tmp_path / "f.elf", cache
            )
        assert isinstance(data, dict)
        assert "cc_path" in data


def test_is_launcher_matches_only_known_launchers() -> None:
    """Compilers of any shape pass; only the closed launcher set matches."""
    for token in ("/t/g++-13", "gcc-8.4.0", "clang++-17", "armcc", "icx", "cc"):
        assert not idedata._is_launcher(token)
    for token in ("/opt/homebrew/bin/ccache", "CCACHE.EXE", "distcc", "sccache"):
        assert idedata._is_launcher(token)


def test_load_or_build_idedata_corrupted_cache_is_logged(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A truncated cache is diagnosable, not a silent slow-build cause."""
    compile_commands = _write_compile_commands(tmp_path)
    cache = tmp_path / "c.json"
    cache.write_text('{"cc_path": trunc')
    os.utime(cache, (compile_commands.stat().st_mtime + 5,) * 2)
    with patch.object(idedata, "get_toolchain_includes", return_value=[]):
        data = idedata.load_or_build_idedata(
            compile_commands, tmp_path / "f.elf", cache
        )
    assert data["cxx_path"] == "/tools/g++"
    assert "Discarding unreadable idedata cache" in caplog.text


def test_load_or_build_idedata_never_caches_a_launcher(tmp_path: Path) -> None:
    """A compile DB naming a launcher as the compiler is rejected, never cached."""
    compile_commands = tmp_path / "compile_commands.json"
    compile_commands.write_text(
        json.dumps(
            [
                _entry(
                    f"{ABS}build",
                    f"{ABS}build/src/esphome/core/application.cpp",
                    "/opt/homebrew/bin/ccache -c app.cpp -o app.cpp.o",
                )
            ]
        )
    )
    cache = tmp_path / "c.json"
    # No probe patch needed: the launcher is rejected before the probe runs
    with pytest.raises(EsphomeError, match="compile database is unusable"):
        idedata.load_or_build_idedata(compile_commands, tmp_path / "f.elf", cache)
    assert not cache.exists()


def test_load_or_build_idedata_cache_hit_skips_rebuild(tmp_path: Path) -> None:
    """A valid cache newer than the compile DB is served without re-parsing."""
    compile_commands = _write_compile_commands(tmp_path)
    cache = tmp_path / "c.json"
    cache.write_text(json.dumps({"cc_path": "/tools/gcc", "cached": True}))
    os.utime(cache, (compile_commands.stat().st_mtime + 5,) * 2)
    with patch.object(idedata, "idedata_from_build") as mock_build:
        data = idedata.load_or_build_idedata(
            compile_commands, tmp_path / "f.elf", cache
        )
    mock_build.assert_not_called()
    assert data["cached"] is True

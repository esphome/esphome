"""Tests for the shared extraScript machinery (platformio.extra_script)."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from esphome.platformio.library import ConvertedLibrary as IDFComponent, URLSource


def test_extra_script_captures_libpath_libs_and_defines(tmp_path):
    from esphome.platformio.extra_script import (
        captured_as_build_flags,
        run_extra_script,
    )

    (tmp_path / "src" / "esp32").mkdir(parents=True)
    script = tmp_path / "extra_script.py"
    script.write_text(
        "Import('env')\n"
        "mcu = env.get('BOARD_MCU')\n"
        "env.Append(\n"
        "    LIBPATH=[join('src', mcu)],\n"
        "    LIBS=['algobsec'],\n"
        "    CPPDEFINES=['FOO', ('BAR', '1')],\n"
        "    LINKFLAGS=['-Wl,--gc-sections'],\n"
        ")\n"
    )
    # The script uses bare ``join`` (PIO's extra-scripts run inside SCons
    # where this is in scope). Inject it via the script header so the
    # shim's exec namespace can resolve it.
    script.write_text("from os.path import join\n" + script.read_text())

    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )

    assert result.libpath == [str(Path("src") / "esp32")]
    assert result.libs == ["algobsec"]
    assert ("BAR", "1") in result.cppdefines
    assert "FOO" in result.cppdefines
    assert result.linkflags == ["-Wl,--gc-sections"]

    flags = captured_as_build_flags(result, library_dir=tmp_path)
    sep = os.sep
    assert f"-Lsrc{sep}esp32" in flags
    assert "-lalgobsec" in flags
    assert "-DFOO" in flags
    assert "-DBAR=1" in flags
    assert "-Wl,--gc-sections" in flags


def test_extra_script_libpath_relative_resolves_against_library_dir(
    tmp_path, monkeypatch
):
    """Relative LIBPATH entries must resolve against ``library_dir``, not the
    caller's CWD (the shim restores CWD before ``captured_as_build_flags``
    runs)."""
    from esphome.platformio.extra_script import (
        ExtraScriptResult,
        captured_as_build_flags,
    )

    (tmp_path / "lib" / "esp32").mkdir(parents=True)
    elsewhere = tmp_path.parent / "not_the_library_dir"
    elsewhere.mkdir(exist_ok=True)
    monkeypatch.chdir(elsewhere)

    result = ExtraScriptResult(libpath=["lib/esp32"])
    flags = captured_as_build_flags(result, library_dir=tmp_path)

    sep = os.sep
    assert flags == [f"-Llib{sep}esp32"]


def test_extra_script_libpath_absolute_outside_library_dir(tmp_path):
    from esphome.platformio.extra_script import (
        ExtraScriptResult,
        captured_as_build_flags,
    )

    outside = tmp_path.parent / "system_lib"
    outside.mkdir(exist_ok=True)
    result = ExtraScriptResult(libpath=[str(outside)])

    flags = captured_as_build_flags(result, library_dir=tmp_path)
    assert flags == [f"-L{outside.resolve()}"]


def test_extra_script_failure_returns_empty_result(tmp_path, caplog):
    from esphome.platformio.extra_script import run_extra_script

    script = tmp_path / "broken.py"
    script.write_text("raise RuntimeError('boom')\n")

    with caplog.at_level("WARNING"):
        result = run_extra_script(
            script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
        )

    assert result.libpath == []
    assert result.libs == []
    assert "broken.py" in caplog.text


def test_apply_extra_script_path_traversal_is_rejected(tmp_path):
    from esphome.platformio.extra_script import apply_extra_script

    library_dir = tmp_path / "lib"
    library_dir.mkdir()
    outside = tmp_path / "evil.py"
    outside.write_text("env.Append(LIBS=['pwned'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = library_dir
    c.data = {"build": {"extraScript": "../evil.py"}}

    from esphome.core import EsphomeError

    with pytest.raises(EsphomeError, match="escapes the library directory"):
        apply_extra_script(c, board_mcu=lambda: "esp32", pio_platform="espressif32")
    # Nothing was folded into flags: the traversal was rejected before
    # the script could run.
    assert "flags" not in c.data["build"]


def test_apply_extra_script_merges_into_existing_flags(tmp_path):
    from esphome.platformio.extra_script import apply_extra_script

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=['algobsec'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": ["-DEXISTING"]}}

    apply_extra_script(c, board_mcu=lambda: "esp32", pio_platform="espressif32")

    assert "-DEXISTING" in c.data["build"]["flags"]
    assert "-lalgobsec" in c.data["build"]["flags"]


def test_apply_extra_script_malformed_flags_raises(tmp_path) -> None:
    """A null/dict build.flags fails naming the library instead of injecting
    a non-string into the compiler command line."""
    from esphome.core import EsphomeError
    from esphome.platformio.extra_script import apply_extra_script

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=['algobsec'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": None}}

    with pytest.raises(EsphomeError, match="malformed build.flags"):
        apply_extra_script(c, board_mcu=lambda: "esp32", pio_platform="espressif32")


def test_apply_extra_script_callable_target_and_str_flags(tmp_path) -> None:
    """The shared helper resolves the board_mcu callable lazily and normalizes
    a string ``build.flags`` value into a list before extending it."""
    from esphome.platformio.extra_script import apply_extra_script

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env.get('BOARD_MCU')])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": "-DBASE=1"}}

    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")

    assert c.data["build"]["flags"] == ["-DBASE=1", "-lesp8266"]


def test_apply_extra_script_no_script_and_no_flags(tmp_path) -> None:
    from esphome.platformio.extra_script import apply_extra_script

    # No extraScript declared: nothing happens, the target is never resolved
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {}}
    apply_extra_script(
        c,
        board_mcu=lambda: pytest.fail("target resolved without a script"),
        pio_platform="espressif8266",
    )

    # A script that captures nothing leaves the flags untouched
    script = tmp_path / "noop.py"
    script.write_text("pass\n")
    c.data = {"build": {"extraScript": "noop.py"}}
    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")
    assert "flags" not in c.data["build"]


def test_apply_extra_script_ignores_uncaptured_env_calls(tmp_path, caplog) -> None:
    """Un-captured env vars and unsupported env methods are skipped but
    diagnosable from the build log."""
    import logging

    from esphome.platformio.extra_script import apply_extra_script

    caplog.set_level(logging.DEBUG)
    script = tmp_path / "extra.py"
    script.write_text(
        "env.Replace(CC='clang')\nenv.Append(UNCAPTURED=['x'], LIBS='single')\n"
    )
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")
    assert c.data["build"]["flags"] == ["-lsingle"]
    assert "env.Append(UNCAPTURED=...) is not captured" in caplog.text
    assert "env.Replace(...) is not supported" in caplog.text


def test_apply_extra_script_swallows_script_errors(tmp_path, caplog) -> None:
    """A raising extra-script is best-effort: logged and skipped."""
    from esphome.platformio.extra_script import apply_extra_script

    script = tmp_path / "extra.py"
    script.write_text("raise RuntimeError('boom')\n")
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")
    assert "flags" not in c.data["build"]
    assert "ignoring its output" in caplog.text


def test_apply_extra_script_pio_platform(tmp_path) -> None:
    """The backend's platform token is exposed to the script as PIOPLATFORM."""
    from esphome.platformio.extra_script import apply_extra_script

    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env.get('PIOPLATFORM')])\n")
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")
    assert c.data["build"]["flags"] == ["-lespressif8266"]


def test_apply_extra_script_missing_script_raises(tmp_path) -> None:
    """A declared but absent extraScript is a broken package and fails by
    name, as it would under PlatformIO."""
    from esphome.core import EsphomeError
    from esphome.platformio.extra_script import apply_extra_script

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "nope.py"}}
    with pytest.raises(EsphomeError, match="nope.py of library owner/name not found"):
        apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")


def test_run_extra_script_failure_discards_partial_capture(tmp_path, caplog) -> None:
    """A crashed script yields an empty result: half-applied flags could
    build wrong-output firmware that links cleanly."""
    from esphome.platformio.extra_script import run_extra_script

    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=['algobsec'])\nraise RuntimeError('boom')\n")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == []
    assert "ignoring its output" in caplog.text


def test_run_extra_script_syntax_error_is_best_effort(tmp_path, caplog) -> None:
    """A vendored script that does not even compile warns and skips instead
    of aborting the build."""
    from esphome.platformio.extra_script import run_extra_script

    script = tmp_path / "extra.py"
    script.write_text("def broken(:\n")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == []
    assert "ignoring its output" in caplog.text


def test_unsupported_env_method_warns_once(caplog) -> None:
    """Repeated calls to the same unsupported method warn only once."""
    from esphome.platformio.extra_script import _FakeSConsEnv

    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    env.Replace(CC="clang")
    env.Replace(CC="gcc")
    assert caplog.text.count("env.Replace(...) is not supported") == 1


def test_run_extra_script_sys_exit_is_best_effort(tmp_path, caplog) -> None:
    """sys.exit() in a vendored script must not kill the esphome run."""
    from esphome.platformio.extra_script import run_extra_script

    script = tmp_path / "extra.py"
    script.write_text("import sys\nsys.exit(3)\n")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == []
    assert "ignoring its output" in caplog.text

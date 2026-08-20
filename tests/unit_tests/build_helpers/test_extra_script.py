"""Tests for the shared extraScript machinery (build_helpers.extra_script)."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from esphome.platformio.library import ConvertedLibrary as IDFComponent, URLSource


def test_extra_script_captures_libpath_libs_and_defines(tmp_path):
    from esphome.build_helpers.extra_script import (
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

    result = run_extra_script(script, library_dir=tmp_path, idf_target="esp32")

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
    from esphome.build_helpers.extra_script import (
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
    from esphome.build_helpers.extra_script import (
        ExtraScriptResult,
        captured_as_build_flags,
    )

    outside = tmp_path.parent / "system_lib"
    outside.mkdir(exist_ok=True)
    result = ExtraScriptResult(libpath=[str(outside)])

    flags = captured_as_build_flags(result, library_dir=tmp_path)
    assert flags == [f"-L{outside.resolve()}"]


def test_extra_script_failure_returns_empty_result(tmp_path, caplog):
    from esphome.build_helpers.extra_script import run_extra_script

    script = tmp_path / "broken.py"
    script.write_text("raise RuntimeError('boom')\n")

    with caplog.at_level("WARNING"):
        result = run_extra_script(script, library_dir=tmp_path, idf_target="esp32")

    assert result.libpath == []
    assert result.libs == []
    assert "broken.py" in caplog.text


def test_apply_extra_script_path_traversal_is_rejected(tmp_path):
    from esphome.espidf.component import _apply_extra_script

    library_dir = tmp_path / "lib"
    library_dir.mkdir()
    outside = tmp_path / "evil.py"
    outside.write_text("env.Append(LIBS=['pwned'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = library_dir
    c.data = {"build": {"extraScript": "../evil.py"}}

    _apply_extra_script(c)

    # Nothing was folded into flags: the traversal was rejected before
    # the script could run.
    assert "flags" not in c.data["build"]


def test_apply_extra_script_merges_into_existing_flags(tmp_path, monkeypatch):
    from esphome.components import esp32 as esp32_module

    monkeypatch.setattr(esp32_module, "get_esp32_variant", lambda: "ESP32")

    from esphome.espidf.component import _apply_extra_script

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=['algobsec'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": ["-DEXISTING"]}}

    _apply_extra_script(c)

    assert "-DEXISTING" in c.data["build"]["flags"]
    assert "-lalgobsec" in c.data["build"]["flags"]


def test_apply_extra_script_callable_target_and_str_flags(tmp_path) -> None:
    """The shared helper resolves a callable idf_target lazily and normalizes
    a string ``build.flags`` value into a list before extending it."""
    from esphome.build_helpers.extra_script import apply_extra_script

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env.get('BOARD_MCU')])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": "-DBASE=1"}}

    apply_extra_script(c, lambda: "esp8266")

    assert c.data["build"]["flags"] == ["-DBASE=1", "-lesp8266"]


def test_apply_extra_script_no_script_and_no_flags(tmp_path) -> None:
    from esphome.build_helpers.extra_script import apply_extra_script

    # No extraScript declared: nothing happens, the target is never resolved
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {}}
    apply_extra_script(c, lambda: pytest.fail("target resolved without a script"))

    # A script that captures nothing leaves the flags untouched
    script = tmp_path / "noop.py"
    script.write_text("pass\n")
    c.data = {"build": {"extraScript": "noop.py"}}
    apply_extra_script(c, "esp8266")
    assert "flags" not in c.data["build"]


def test_apply_extra_script_ignores_uncaptured_env_calls(tmp_path) -> None:
    """Un-captured env vars and unsupported env methods are silent no-ops."""
    from esphome.build_helpers.extra_script import apply_extra_script

    script = tmp_path / "extra.py"
    script.write_text(
        "env.Replace(CC='clang')\nenv.Append(UNCAPTURED=['x'], LIBS='single')\n"
    )
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    apply_extra_script(c, "esp8266")
    assert c.data["build"]["flags"] == ["-lsingle"]


def test_apply_extra_script_swallows_script_errors(tmp_path, caplog) -> None:
    """A raising extra-script is best-effort: logged and skipped."""
    from esphome.build_helpers.extra_script import apply_extra_script

    script = tmp_path / "extra.py"
    script.write_text("raise RuntimeError('boom')\n")
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    apply_extra_script(c, "esp8266")
    assert "flags" not in c.data["build"]
    assert "skipping" in caplog.text


def test_apply_extra_script_pio_platform(tmp_path) -> None:
    """The backend's platform token is exposed to the script as PIOPLATFORM."""
    from esphome.build_helpers.extra_script import apply_extra_script

    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env.get('PIOPLATFORM')])\n")
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    apply_extra_script(c, "esp8266", pio_platform="espressif8266")
    assert c.data["build"]["flags"] == ["-lespressif8266"]


def test_apply_extra_script_missing_script_logged(tmp_path, caplog) -> None:
    """A declared but absent extraScript is skipped with a visible warning:
    its captured link flags are lost."""
    from esphome.build_helpers.extra_script import apply_extra_script

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "nope.py"}}
    apply_extra_script(c, "esp8266")
    assert "not found" in caplog.text

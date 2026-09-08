"""Tests for the shared extraScript machinery (platformio.extra_script)."""

from __future__ import annotations

import logging
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.core import EsphomeError
from esphome.platformio.extra_script import (
    CppDefine,
    ExtraScriptResult,
    _FakeSConsEnv,
    apply_extra_script,
    captured_as_build_flags,
    run_extra_script,
)
from esphome.platformio.library import (
    ESPHOME_DATA_KEY,
    ESPHOME_DATA_LINK_FLAGS_KEY,
    ConvertedLibrary as IDFComponent,
    URLSource,
    lex_build_flags,
)


def test_extra_script_captures_libpath_libs_and_defines(tmp_path):

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
    assert CppDefine("BAR", "1") in result.cppdefines
    assert CppDefine("FOO") in result.cppdefines
    assert result.linkflags == ["-Wl,--gc-sections"]

    # Lex like the consumer does: quoting makes raw strings platform-varying
    tokens = lex_build_flags(
        captured_as_build_flags(result, library_dir=tmp_path), "test"
    )
    sep = os.sep
    assert f"-Lsrc{sep}esp32" in tokens
    assert "-lalgobsec" in tokens
    assert "-DFOO" in tokens
    assert "-DBAR=1" in tokens
    # LINKFLAGS travel via the link-flags channel, never the compile flags
    assert "-Wl,--gc-sections" not in tokens


def test_extra_script_libpath_relative_resolves_against_library_dir(
    tmp_path, monkeypatch
):
    """Relative LIBPATH entries must resolve against ``library_dir``, not the
    caller's CWD (the shim restores CWD before ``captured_as_build_flags``
    runs)."""

    (tmp_path / "lib" / "esp32").mkdir(parents=True)
    elsewhere = tmp_path.parent / "not_the_library_dir"
    elsewhere.mkdir(exist_ok=True)
    monkeypatch.chdir(elsewhere)

    result = ExtraScriptResult(libpath=["lib/esp32"])
    flags = captured_as_build_flags(result, library_dir=tmp_path)

    sep = os.sep
    assert lex_build_flags(flags, "test") == [f"-Llib{sep}esp32"]


def test_extra_script_libpath_absolute_outside_library_dir(tmp_path):

    outside = tmp_path.parent / "system_lib"
    outside.mkdir(exist_ok=True)
    result = ExtraScriptResult(libpath=[str(outside)])

    flags = captured_as_build_flags(result, library_dir=tmp_path)
    assert lex_build_flags(flags, "test") == [f"-L{outside.resolve()}"]


def test_extra_script_failure_returns_empty_result(tmp_path, caplog):

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

    library_dir = tmp_path / "lib"
    library_dir.mkdir()
    outside = tmp_path / "evil.py"
    outside.write_text("env.Append(LIBS=['pwned'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = library_dir
    c.data = {"build": {"extraScript": "../evil.py"}}

    with pytest.raises(EsphomeError, match="escapes the library directory"):
        apply_extra_script(c, board_mcu=lambda: "esp32", pio_platform="espressif32")
    # Nothing was folded into flags: the traversal was rejected before
    # the script could run.
    assert "flags" not in c.data["build"]


def test_apply_extra_script_merges_into_existing_flags(tmp_path):

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

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env.get('BOARD_MCU')])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": "-DBASE=1"}}

    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")

    assert c.data["build"]["flags"] == ["-DBASE=1", "-lesp8266"]


def test_captured_nonstring_buckets_warn_and_skip(tmp_path, caplog) -> None:
    """Non-string LIBS/LINKFLAGS/CPPFLAGS/LIBPATH entries (legal SCons
    nodes) are skipped by name instead of stringified into garbage flags."""
    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text(
        "env.Append(LIBS=['m', 42], LINKFLAGS=['-Wl,-x', {'no': 1}], "
        "CPPFLAGS=['-Os', 3.5], LIBPATH=['libs', 7])\n"
    )
    (tmp_path / "libs").mkdir()

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}

    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")

    flags = c.data["build"]["flags"]
    assert "-lm" in flags and "-Os" in flags
    assert c.data[ESPHOME_DATA_KEY][ESPHOME_DATA_LINK_FLAGS_KEY] == ["-Wl,-x"]
    assert not any("42" in f or "no" in f or "3.5" in f for f in flags)
    assert "Ignoring unsupported LIBS entry 42" in caplog.text
    assert "Ignoring unsupported LINKFLAGS entry {'no': 1}" in caplog.text
    assert "Ignoring unsupported LIBPATH entry 7" in caplog.text


def test_captured_dict_cppdefines_warn_and_skip(tmp_path, caplog) -> None:
    """A dict CPPDEFINES entry (legal SCons) must warn and skip; formatting
    it blind would hand the compiler -D{'FOO': '1'} garbage."""
    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text(
        "env.Append(CPPDEFINES=[{'FOO': '1'}, ('BAR', 2), ['BAZ', 3], 'PLAIN'])\n"
    )

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}

    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")

    assert c.data["build"]["flags"] == ["-DBAR=2", "-DBAZ=3", "-DPLAIN"]
    assert "Ignoring unsupported CPPDEFINES entry" in caplog.text


def test_apply_extra_script_subscript_env_read(tmp_path) -> None:
    """Scripts also read env["BOARD_MCU"]; the subscript form must work or
    the broad handler discards every flag the script captured."""
    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env['BOARD_MCU']])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}

    apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")

    assert c.data["build"]["flags"] == ["-lesp8266"]


def test_apply_extra_script_no_script_and_no_flags(tmp_path) -> None:

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
    assert "env.Replace is not supported" in caplog.text


def test_apply_extra_script_swallows_script_errors(tmp_path, caplog) -> None:
    """A raising extra-script is best-effort: logged and skipped."""

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

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "nope.py"}}
    with pytest.raises(EsphomeError, match="nope.py of library owner/name not found"):
        apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")


@pytest.mark.parametrize("bad", (["a.py"], {"esp32": "a.py"}), ids=("list", "dict"))
def test_apply_extra_script_non_string_raises(tmp_path, bad) -> None:
    """A non-string extraScript fails naming the library, not with a TypeError."""

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": bad}}
    with pytest.raises(EsphomeError, match="of library owner/name must be a string"):
        apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")


def test_extra_script_cpppath_captured_as_include_flags(tmp_path, monkeypatch):
    """CPPPATH entries translate to -I flags anchored like LIBPATH."""

    (tmp_path / "include").mkdir()
    outside = tmp_path.parent / "system_inc"
    outside.mkdir(exist_ok=True)
    elsewhere = tmp_path.parent / "not_the_library_dir"
    elsewhere.mkdir(exist_ok=True)
    monkeypatch.chdir(elsewhere)

    result = ExtraScriptResult(cpppath=["include", str(outside), 7])
    flags = captured_as_build_flags(result, library_dir=tmp_path)

    assert lex_build_flags(flags, "test") == ["-Iinclude", f"-I{outside.resolve()}"]


def test_extra_script_spaced_paths_survive_relexing(tmp_path):
    """-I/-L paths with spaces round-trip through lex_build_flags as one token."""
    (tmp_path / "my libs").mkdir()
    result = ExtraScriptResult(cpppath=["my libs"], libpath=["my libs"])
    flags = captured_as_build_flags(result, library_dir=tmp_path)
    assert lex_build_flags(flags, "test") == ["-Imy libs", "-Lmy libs"]


def test_run_extra_script_failure_discards_partial_capture(tmp_path, caplog) -> None:
    """A crashed script yields an empty result: half-applied flags could
    build wrong-output firmware that links cleanly."""

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

    script = tmp_path / "extra.py"
    script.write_text("def broken(:\n")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == []
    assert "ignoring its output" in caplog.text


def test_unsupported_env_method_warns_once(caplog) -> None:
    """Repeated calls to the same unsupported method warn only once."""

    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    env.Replace(CC="clang")
    env.Replace(CC="gcc")
    assert caplog.text.count("env.Replace is not supported") == 1


def test_run_extra_script_sys_exit_is_best_effort(tmp_path, caplog) -> None:
    """A nonzero sys.exit() in a vendored script must not kill the esphome
    run, and its output is discarded."""

    script = tmp_path / "extra.py"
    script.write_text("import sys\nenv.Append(LIBS=['x'])\nsys.exit(3)\n")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == []
    assert "exited with status 3" in caplog.text


def test_run_extra_script_sys_exit_zero_is_success(tmp_path, caplog) -> None:
    """sys.exit(0) is a normal PlatformIO script ending: the capture is kept."""

    script = tmp_path / "extra.py"
    script.write_text("import sys\nenv.Append(LIBS=['algobsec'])\nsys.exit(0)\n")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == ["algobsec"]
    assert "ignoring its output" not in caplog.text


def test_run_extra_script_unreadable_raises(tmp_path) -> None:
    """An unreadable declared script is a broken package, like a missing one."""

    script = tmp_path / "extra.py"
    script.write_text("")
    with (
        patch("pathlib.Path.read_text", side_effect=OSError("denied")),
        pytest.raises(EsphomeError, match="is unreadable"),
    ):
        run_extra_script(
            script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
        )


def test_run_extra_script_bad_encoding_is_best_effort(tmp_path, caplog) -> None:
    """Undecodable content warns and skips, like a SyntaxError."""

    script = tmp_path / "extra.py"
    script.write_bytes(b"\xff\xfe\x00bad")
    result = run_extra_script(
        script, library_dir=tmp_path, board_mcu="esp32", pio_platform="espressif32"
    )
    assert result.libs == []
    assert "is not UTF-8" in caplog.text


@pytest.mark.parametrize("method", ("Prepend", "AppendUnique", "PrependUnique"))
def test_append_variants_capture_like_append(method: str) -> None:
    """Prepend/AppendUnique/PrependUnique write the captured keys too."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    getattr(env, method)(LIBS=["algobsec"], LIBPATH=["lib"])
    assert env.result.libs == ["algobsec"]
    assert env.result.libpath == ["lib"]


@pytest.mark.parametrize("method", ("Prepend", "PrependUnique"))
def test_prepend_inserts_ahead_of_existing(method: str) -> None:
    """Prepend keeps SCons order: new values land ahead of what is already
    captured (scripts prepend LIBS for static-link symbol resolution)."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    env.Append(LIBS=["m"])
    getattr(env, method)(LIBS=["algobsec", "bsec"])
    assert env.result.libs == ["algobsec", "bsec", "m"]


def test_env_membership_and_iteration(tmp_path) -> None:
    """Membership tests and for-loops must use the mapping protocol; the
    legacy sequence fallback through __getitem__ would loop forever."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    assert "BOARD_MCU" in env
    assert "NOPE" not in env
    assert sorted(env) == ["BOARD_MCU", "PIOENV", "PIOPLATFORM"]


def test_apply_extra_script_non_string_falsey_raises(tmp_path) -> None:
    """A falsey non-string extraScript (false, 0, []) is a malformed
    manifest, not an absent script."""
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": False}}
    with pytest.raises(EsphomeError, match="must be a string"):
        apply_extra_script(c, board_mcu=lambda: "esp8266", pio_platform="espressif8266")


def test_env_get_unknown_key_warns_once(caplog) -> None:
    """A script branching on an unmodelled env var is diagnosable."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    assert env.get("BOARD") is None
    assert env.get("BOARD", "d1") == "d1"
    assert env.get("BOARD_MCU") == "esp8266"
    assert caplog.text.count("env.get('BOARD') is not modelled") == 1
    assert "BOARD_MCU" not in caplog.text


def test_spaced_cppflag_survives_relexing(tmp_path) -> None:
    """A captured argv token with a space stays one token after lexing."""
    result = ExtraScriptResult(
        cppflags=["-include my hdr.h"],
        cppdefines=[CppDefine("MSG", '"hello world"'), CppDefine("PLAIN")],
    )
    flags = captured_as_build_flags(result, library_dir=tmp_path)
    assert lex_build_flags(flags, "test") == [
        '-DMSG="hello world"',
        "-DPLAIN",
        "-include my hdr.h",
    ]


def test_env_attribute_access_warns_without_call(caplog) -> None:
    """hasattr()/truthiness on an unsupported method is diagnosable; dunder
    protocol probes stay silent."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    assert env.GetProjectOption
    assert caplog.text.count("env.GetProjectOption is not supported") == 1
    assert not hasattr(env, "__deepcopy__")
    assert "__deepcopy__" not in caplog.text


def test_env_unmodelled_subscript_degrades_one_branch(caplog) -> None:
    """env[...] on an unmodelled var returns '' instead of KeyError
    discarding the whole capture."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    assert env["PIOFRAMEWORK"] == ""
    assert env["PIOFRAMEWORK"] == ""
    assert caplog.text.count("env['PIOFRAMEWORK'] is not modelled") == 1
    assert env["BOARD_MCU"] == "esp8266"
    env.Append(LIBS=["still_captured"])
    assert env.result.libs == ["still_captured"]


def test_cppdefines_scons_spellings(tmp_path) -> None:
    """A bare 2-tuple is one name=value pair, a dict maps names to values,
    and a None value is a bare define (SCons processDefines)."""
    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    env.Append(CPPDEFINES=("FOO", "1"))
    env.Append(CPPDEFINES={"BAR": "2", "BAZ": None})
    env.Append(CPPDEFINES=["PLAIN"])
    flags = captured_as_build_flags(env.result, library_dir=tmp_path)
    assert lex_build_flags(flags, "test") == [
        "-DFOO=1",
        "-DBAR=2",
        "-DBAZ",
        "-DPLAIN",
    ]


def test_uncaptured_append_key_warns_once(caplog) -> None:
    """A loop of Appends to the same uncaptured key warns once."""

    env = _FakeSConsEnv(
        board_mcu="esp8266", pio_env="esphome_esp8266", pio_platform="espressif8266"
    )
    env.Append(RANLIBFLAGS=["a"])
    env.Append(RANLIBFLAGS=["b"])
    assert caplog.text.count("env.Append(RANLIBFLAGS=...) is not captured") == 1

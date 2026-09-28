"""Tests for esphome.host.toolchain (the native host build driver)."""

from __future__ import annotations

from collections.abc import Generator
import logging
import os
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
    Toolchain,
)
from esphome.core import CORE, EsphomeError
from esphome.host import toolchain
from esphome.host.toolchain import PROGRAM_NAME


@pytest.fixture(autouse=True)
def _core(tmp_path: Path) -> None:
    CORE.config_path = tmp_path / "dev.yaml"
    CORE.build_path = tmp_path
    CORE.name = "dev"
    CORE.toolchain = Toolchain.HOST
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_HOST}


def _abs(path: str) -> str:
    """What the toolchain makes of a tool path (a drive is added on Windows)."""
    return str(Path(path).absolute())


def test_find_command_prefers_env_override(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("CXX", "/opt/clang++")
    with patch("shutil.which", side_effect={"/opt/clang++": "/opt/clang++"}.get):
        assert toolchain.find_command("CXX", ("g++",)) == (_abs("/opt/clang++"),)


def test_find_command_keeps_the_override_arguments(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """``CC="gcc -m32"`` is a command, as make and CMake read it."""
    monkeypatch.setenv("CC", "gcc -m32 '--sysroot=/opt/my sdk'")
    with patch("shutil.which", side_effect={"gcc": "/usr/bin/gcc"}.get):
        assert toolchain.find_command("CC", ("cc",)) == (
            _abs("/usr/bin/gcc"),
            "-m32",
            "--sysroot=/opt/my sdk",
        )


@pytest.mark.parametrize(
    ("override", "expected_args"),
    [("ccache gcc", ()), ("/opt/bin/ccache gcc -m32", ("-m32",))],
)
def test_find_command_drops_a_ccache_prefix(
    monkeypatch: pytest.MonkeyPatch, override: str, expected_args: tuple[str, ...]
) -> None:
    """The build adds ccache itself; the compiler is the word after it."""
    monkeypatch.setenv("CC", override)
    with patch("shutil.which", side_effect={"gcc": "/usr/bin/gcc"}.get):
        assert toolchain.find_command("CC", ("cc",)) == (
            _abs("/usr/bin/gcc"),
            *expected_args,
        )


def test_find_command_accepts_a_compiler_named_ccache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Alone, the word is the program: ccache's compiler links work that way."""
    monkeypatch.setenv("CC", "ccache")
    with patch("shutil.which", side_effect={"ccache": "/usr/bin/ccache"}.get):
        assert toolchain.find_command("CC", ("cc",)) == (_abs("/usr/bin/ccache"),)


def test_find_command_env_override_must_run(monkeypatch: pytest.MonkeyPatch) -> None:
    """A broken override fails by name instead of silently using another compiler."""
    monkeypatch.setenv("CXX", "nope++")
    with (
        patch("shutil.which", return_value=None),
        pytest.raises(EsphomeError, match="CXX='nope\\+\\+' does not name"),
    ):
        toolchain.find_command("CXX", ("g++",))


def test_find_command_blank_override_is_unset(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("CC", "  ")
    with patch("shutil.which", side_effect={"clang": "/usr/bin/clang"}.get):
        assert toolchain.find_command("CC", ("gcc", "clang")) == (
            _abs("/usr/bin/clang"),
        )


def test_find_command_first_candidate_wins(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("CC", raising=False)
    table = {"gcc": "/usr/bin/gcc", "clang": "/usr/bin/clang"}
    with patch("shutil.which", side_effect=table.get):
        assert toolchain.find_command("CC", ("gcc", "clang")) == (_abs("/usr/bin/gcc"),)


def test_find_command_none_found(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("CC", raising=False)
    with (
        patch("shutil.which", return_value=None),
        pytest.raises(
            EsphomeError,
            match=r"gcc not found on PATH \(tried gcc, clang\); install it or set CC",
        ),
    ):
        toolchain.find_command("CC", ("gcc", "clang"))


def test_find_command_makes_a_relative_hit_absolute(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Anchor the path: ninja runs from the build directory."""
    monkeypatch.chdir(tmp_path)
    monkeypatch.setenv("CC", "./toolchain/gcc")
    with patch("shutil.which", return_value="./toolchain/gcc"):
        (found,) = toolchain.find_command("CC", ("gcc",))
    assert Path(found) == Path("toolchain/gcc").absolute()


def test_find_tool_refuses_arguments(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("AR", "ar --plugin x")
    with (
        patch("shutil.which", return_value="/usr/bin/ar"),
        pytest.raises(EsphomeError, match="AR must name a program without arguments"),
    ):
        toolchain.find_tool("AR", ("ar",))


def test_find_compilers(monkeypatch: pytest.MonkeyPatch) -> None:
    for var in ("CC", "CXX"):
        monkeypatch.delenv(var, raising=False)
    table = {"gcc": "/usr/bin/gcc", "g++": "/usr/bin/g++"}
    with patch("shutil.which", side_effect=table.get):
        assert toolchain.find_compilers() == toolchain.HostCompilers(
            cc=(_abs("/usr/bin/gcc"),), cxx=(_abs("/usr/bin/g++"),)
        )


def test_build_paths(tmp_path: Path) -> None:
    """The PlatformIO layout is kept: CORE.firmware_bin resolves the same file."""
    assert toolchain.get_build_dir() == tmp_path / ".pioenvs" / "dev"
    assert toolchain.get_elf_path() == CORE.firmware_bin


def test_binutils_paths(monkeypatch: pytest.MonkeyPatch) -> None:
    for var in ("OBJDUMP", "READELF"):
        monkeypatch.delenv(var, raising=False)
    table = {"objdump": "/usr/bin/objdump", "readelf": "/usr/bin/readelf"}
    with patch("shutil.which", side_effect=table.get):
        assert toolchain.get_objdump_path() == Path(_abs("/usr/bin/objdump"))
        assert toolchain.get_readelf_path() == Path(_abs("/usr/bin/readelf"))


def test_get_build_env_merges_without_leaking(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_HOST_PREFIX", str(tmp_path / "cache"))
    monkeypatch.delenv("CCACHE_DIR", raising=False)
    monkeypatch.setenv("KEEP_ME", "1")
    env = toolchain.get_build_env("/usr/bin/ccache")
    assert env["KEEP_ME"] == "1"
    assert env["CCACHE_DIR"] == str((tmp_path / "cache").resolve() / "ccache")
    assert "CCACHE_DIR" not in os.environ


def test_get_build_env_without_ccache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("CCACHE_DIR", raising=False)
    assert "CCACHE_DIR" not in toolchain.get_build_env(None)


@pytest.fixture
def compile_env(tmp_path: Path) -> Generator[SimpleNamespace]:
    """Stub everything run_compile resolves; the build dir holds a manifest."""
    build_dir = tmp_path / ".pioenvs" / "dev"
    build_dir.mkdir(parents=True)
    (build_dir / "build.ninja").write_text("rule x\n")
    compilers = toolchain.HostCompilers(("gcc",), ("g++",))
    with (
        patch.object(toolchain, "find_ninja", return_value=Path("/usr/bin/ninja")),
        patch.object(toolchain, "find_compilers", return_value=compilers),
        patch.object(toolchain, "resolve_absolute_ccache_path", return_value=None),
        patch("esphome.build_gen.host.write_project", return_value=True) as project,
        patch.object(toolchain, "refresh_compile_commands") as refresh,
        patch.object(
            toolchain, "_load_idedata", return_value={"cc_path": "gcc"}
        ) as ide,
        patch("subprocess.run") as run,
    ):
        yield SimpleNamespace(
            build_dir=build_dir,
            compilers=compilers,
            write_project=project,
            refresh=refresh,
            idedata=ide,
            run=run,
        )


def _completed(rc: int = 0) -> SimpleNamespace:
    return SimpleNamespace(returncode=rc, stdout="", stderr="")


def test_run_compile_builds_and_reports_success(compile_env: SimpleNamespace) -> None:
    elf = compile_env.build_dir / PROGRAM_NAME

    def build(cmd: list[str], **kwargs: object) -> SimpleNamespace:
        elf.write_text("")
        return _completed()

    compile_env.run.side_effect = build
    config = {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 4}}
    assert toolchain.run_compile(config, verbose=True) == 0

    compile_env.write_project.assert_called_once_with(compile_env.compilers, None)
    compile_env.refresh.assert_called_once()
    assert compile_env.refresh.call_args.args[3] is True
    compile_env.run.assert_called_once()
    assert compile_env.run.call_args.args[0] == [
        str(Path("/usr/bin/ninja")),
        "-v",
        "-j",
        "4",
        PROGRAM_NAME,
    ]
    assert compile_env.run.call_args.kwargs["cwd"] == compile_env.build_dir
    compile_env.idedata.assert_called_once_with(None)


def test_run_compile_defaults(compile_env: SimpleNamespace) -> None:
    """No verbosity and no process limit: just the ninja target."""
    (compile_env.build_dir / PROGRAM_NAME).write_text("")
    compile_env.run.return_value = _completed()
    assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert compile_env.run.call_args.args[0] == [
        str(Path("/usr/bin/ninja")),
        PROGRAM_NAME,
    ]
    # The build inherits stdio and never captures: progress must stream
    kwargs = compile_env.run.call_args.kwargs
    assert kwargs["check"] is False
    assert kwargs["close_fds"] is False
    assert "capture_output" not in kwargs


def test_run_compile_warns_about_dropped_platformio_options(
    compile_env: SimpleNamespace, caplog: pytest.LogCaptureFixture
) -> None:
    (compile_env.build_dir / PROGRAM_NAME).write_text("")
    compile_env.run.return_value = _completed()
    CORE.platformio_options = {"lib_ignore": ["x"], "board_build.f_cpu": "1"}
    with caplog.at_level(logging.WARNING):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert "platformio_options->board_build.f_cpu is ignored" in caplog.text
    assert "native 'host' toolchain" in caplog.text
    assert "lib_ignore" not in caplog.text


def test_run_compile_passes_the_resolved_ccache(compile_env: SimpleNamespace) -> None:
    (compile_env.build_dir / PROGRAM_NAME).write_text("")
    compile_env.run.return_value = _completed()
    with patch.object(
        toolchain, "resolve_absolute_ccache_path", return_value="/usr/bin/ccache"
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    compile_env.write_project.assert_called_once_with(
        compile_env.compilers, "/usr/bin/ccache"
    )
    compile_env.idedata.assert_called_once_with("/usr/bin/ccache")


def test_run_compile_build_failure_returns_code(compile_env: SimpleNamespace) -> None:
    compile_env.run.return_value = _completed(rc=3)
    assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 3
    compile_env.idedata.assert_not_called()


def test_run_compile_missing_program_fails(
    compile_env: SimpleNamespace, caplog: pytest.LogCaptureFixture
) -> None:
    """A green ninja run that produced no program fails by name."""
    compile_env.run.return_value = _completed()
    with caplog.at_level(logging.ERROR):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 1
    assert "Build produced no" in caplog.text
    compile_env.idedata.assert_not_called()


@pytest.mark.parametrize("ccache", ["/usr/bin/ccache", None])
def test_get_idedata_resolves_ccache(tmp_path: Path, ccache: str | None) -> None:
    with (
        patch.object(toolchain, "resolve_absolute_ccache_path", return_value=ccache),
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata", return_value={"x": 1}
        ) as load,
    ):
        assert toolchain.get_idedata() == {"x": 1}
    load.assert_called_once_with(
        tmp_path / ".pioenvs" / "dev" / "compile_commands.json",
        tmp_path / ".pioenvs" / "dev" / PROGRAM_NAME,
        CORE.relative_internal_path("idedata", "dev.json"),
        launcher=ccache,
    )


@pytest.mark.parametrize("platform", ["darwin", "win32"])
def test_check_analysis_supported_refuses_non_elf(platform: str) -> None:
    with (
        patch.object(toolchain.sys, "platform", platform),
        pytest.raises(EsphomeError, match=f"the host build on {platform}"),
    ):
        toolchain.check_analysis_supported()


def test_check_analysis_supported_accepts_linux() -> None:
    with patch.object(toolchain.sys, "platform", "linux"):
        toolchain.check_analysis_supported()

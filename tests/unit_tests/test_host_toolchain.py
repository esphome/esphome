"""Tests for esphome.host.toolchain (the native host build driver)."""

from __future__ import annotations

import json
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
)
from esphome.core import CORE, EsphomeError
from esphome.host import toolchain


@pytest.fixture(autouse=True)
def _core(tmp_path: Path) -> None:
    CORE.config_path = tmp_path / "dev.yaml"
    CORE.build_path = tmp_path
    CORE.name = "dev"
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_HOST}


def _which(table: dict[str, str]):
    return table.get


def test_find_tool_prefers_env_override(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("CXX", "/opt/clang++")
    with patch("shutil.which", side_effect=_which({"/opt/clang++": "/opt/clang++"})):
        assert toolchain.find_tool("CXX", ("g++",)) == "/opt/clang++"


def test_find_tool_env_override_must_run(monkeypatch: pytest.MonkeyPatch) -> None:
    """A broken override fails by name instead of silently using another compiler."""
    monkeypatch.setenv("CXX", "nope++")
    with (
        patch("shutil.which", return_value=None),
        pytest.raises(EsphomeError, match="CXX='nope\\+\\+' does not name"),
    ):
        toolchain.find_tool("CXX", ("g++",))


def test_find_tool_blank_override_is_unset(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("CC", "  ")
    with patch("shutil.which", side_effect=_which({"clang": "/usr/bin/clang"})):
        assert toolchain.find_tool("CC", ("gcc", "clang")) == "/usr/bin/clang"


def test_find_tool_first_candidate_wins(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("CC", raising=False)
    table = {"gcc": "/usr/bin/gcc", "clang": "/usr/bin/clang"}
    with patch("shutil.which", side_effect=_which(table)):
        assert toolchain.find_tool("CC", ("gcc", "clang")) == "/usr/bin/gcc"


def test_find_tool_none_found(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("CC", raising=False)
    with (
        patch("shutil.which", return_value=None),
        pytest.raises(
            EsphomeError,
            match=r"gcc not found on PATH \(tried gcc, clang\); install it or set CC",
        ),
    ):
        toolchain.find_tool("CC", ("gcc", "clang"))


def test_find_compilers(monkeypatch: pytest.MonkeyPatch) -> None:
    for var in ("CC", "CXX"):
        monkeypatch.delenv(var, raising=False)
    table = {"gcc": "/usr/bin/gcc", "g++": "/usr/bin/g++"}
    with patch("shutil.which", side_effect=_which(table)):
        assert toolchain.find_compilers() == toolchain.HostCompilers(
            cc="/usr/bin/gcc", cxx="/usr/bin/g++"
        )


def test_build_paths(tmp_path: Path) -> None:
    """The PlatformIO layout is kept: CORE.firmware_bin resolves the same file."""
    assert toolchain.get_build_dir() == tmp_path / ".pioenvs" / "dev"
    assert toolchain.get_elf_path() == tmp_path / ".pioenvs" / "dev" / "program"
    assert toolchain.get_elf_path() == CORE.firmware_bin


def test_binutils_paths(monkeypatch: pytest.MonkeyPatch) -> None:
    for var in ("OBJDUMP", "READELF"):
        monkeypatch.delenv(var, raising=False)
    table = {"objdump": "/usr/bin/objdump", "readelf": "/usr/bin/readelf"}
    with patch("shutil.which", side_effect=_which(table)):
        assert toolchain.get_objdump_path() == Path("/usr/bin/objdump")
        assert toolchain.get_readelf_path() == Path("/usr/bin/readelf")


def test_ccache_env_disabled() -> None:
    assert toolchain.ccache_env(None) == {}


def test_ccache_env_uses_host_cache(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_HOST_PREFIX", str(tmp_path / "cache"))
    for key in ("CCACHE_DIR", "CCACHE_BASEDIR", "CCACHE_DEPEND", "CCACHE_NOHASHDIR"):
        monkeypatch.delenv(key, raising=False)
    env = toolchain.ccache_env("/usr/bin/ccache")
    assert env["CCACHE_DIR"] == str((tmp_path / "cache").resolve() / "ccache")
    assert env["CCACHE_BASEDIR"] == str(tmp_path.resolve())


def test_get_build_env_merges_without_leaking(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_HOST_PREFIX", str(tmp_path / "cache"))
    monkeypatch.delenv("CCACHE_DIR", raising=False)
    monkeypatch.setenv("KEEP_ME", "1")
    env = toolchain.get_build_env("/usr/bin/ccache")
    assert env["KEEP_ME"] == "1"
    assert "CCACHE_DIR" in env
    assert "CCACHE_DIR" not in os.environ


def test_warn_ignored_platformio_options(caplog: pytest.LogCaptureFixture) -> None:
    CORE.platformio_options = {"lib_ignore": ["x"], "board_build.f_cpu": "1"}
    with caplog.at_level(logging.WARNING):
        toolchain._warn_ignored_platformio_options()
    assert "platformio_options->board_build.f_cpu is ignored" in caplog.text
    assert "lib_ignore" not in caplog.text


@pytest.fixture
def compile_env(tmp_path: Path):
    """Stub everything run_compile resolves; the build dir holds a manifest."""
    build_dir = tmp_path / ".pioenvs" / "dev"
    build_dir.mkdir(parents=True)
    (build_dir / "build.ninja").write_text("rule x\n")
    compilers = toolchain.HostCompilers("gcc", "g++")
    with (
        patch.object(toolchain, "find_ninja", return_value=Path("/usr/bin/ninja")),
        patch.object(toolchain, "find_compilers", return_value=compilers),
        patch.object(toolchain, "resolve_ccache_path", return_value=None),
        patch("esphome.build_gen.host.write_project", return_value=True) as project,
        patch.object(toolchain, "_refresh_compile_commands") as refresh,
        patch.object(toolchain, "get_idedata", return_value={"cc_path": "gcc"}) as ide,
        patch("subprocess.run") as run,
    ):
        yield SimpleNamespace(
            build_dir=build_dir,
            write_project=project,
            refresh=refresh,
            idedata=ide,
            run=run,
        )


def _completed(rc: int = 0, stdout: str = "", stderr: str = "") -> SimpleNamespace:
    return SimpleNamespace(returncode=rc, stdout=stdout, stderr=stderr)


def test_run_compile_builds_and_reports_success(compile_env) -> None:
    elf = compile_env.build_dir / "program"

    def build(cmd, **kwargs):
        elf.write_text("")
        return _completed()

    compile_env.run.side_effect = build
    config = {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 4}}
    assert toolchain.run_compile(config, verbose=True) == 0

    compile_env.write_project.assert_called_once_with(
        toolchain.HostCompilers("gcc", "g++"), None
    )
    compile_env.refresh.assert_called_once()
    assert compile_env.refresh.call_args.args[3] is True
    # A changed manifest skips the dry-run probe and builds straight away
    compile_env.run.assert_called_once()
    assert compile_env.run.call_args.args[0] == [
        "/usr/bin/ninja",
        "-v",
        "-j",
        "4",
        "program",
    ]
    assert compile_env.run.call_args.kwargs["cwd"] == compile_env.build_dir
    compile_env.idedata.assert_called_once_with(None)


def test_run_compile_defaults(compile_env) -> None:
    """No verbosity and no process limit: just the ninja target."""
    (compile_env.build_dir / "program").write_text("")
    compile_env.run.return_value = _completed()
    assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert compile_env.run.call_args.args[0] == ["/usr/bin/ninja", "program"]


def test_run_compile_skips_build_when_nothing_to_do(
    compile_env, caplog: pytest.LogCaptureFixture
) -> None:
    compile_env.write_project.return_value = False
    (compile_env.build_dir / "program").write_text("")
    compile_env.run.return_value = _completed(stdout="ninja: no work to do.")
    with caplog.at_level(logging.DEBUG):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    # Only the dry-run probe ran
    compile_env.run.assert_called_once()
    assert compile_env.run.call_args.args[0] == ["/usr/bin/ninja", "-n", "program"]
    assert "nothing to rebuild" in caplog.text
    assert compile_env.refresh.call_args.args[3] is False


def test_run_compile_probe_diagnostics_fall_through_to_build(
    compile_env, caplog: pytest.LogCaptureFixture
) -> None:
    """A failing probe is not trusted: the real build prints the cause."""
    compile_env.write_project.return_value = False
    (compile_env.build_dir / "program").write_text("")
    compile_env.run.side_effect = [
        _completed(rc=1, stderr="ninja: error: multiple rules generate x"),
        _completed(),
    ]
    with caplog.at_level(logging.DEBUG):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert "ninja: ninja: error: multiple rules generate x" in caplog.text
    assert "ninja probe failed" in caplog.text
    assert compile_env.run.call_count == 2


def test_run_compile_probe_with_work_builds(compile_env) -> None:
    compile_env.write_project.return_value = False
    (compile_env.build_dir / "program").write_text("")
    compile_env.run.side_effect = [_completed(stdout="[1/2] CXX x.o"), _completed()]
    assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert compile_env.run.call_count == 2


def test_run_compile_build_failure_returns_code(compile_env) -> None:
    compile_env.run.return_value = _completed(rc=3)
    assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 3
    compile_env.idedata.assert_not_called()


def test_run_compile_missing_program_fails(
    compile_env, caplog: pytest.LogCaptureFixture
) -> None:
    """A green ninja run that produced no program fails by name."""
    compile_env.run.return_value = _completed()
    with caplog.at_level(logging.ERROR):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 1
    assert "Build produced no" in caplog.text
    compile_env.idedata.assert_not_called()


def test_nothing_to_do_reads_stdout(tmp_path: Path) -> None:
    with patch("subprocess.run", return_value=_completed(stdout="[1/3] CC a.o")):
        assert toolchain._nothing_to_do(Path("ninja"), tmp_path, {}) is False
    with patch("subprocess.run", return_value=_completed(stdout="no work to do.")):
        assert toolchain._nothing_to_do(Path("ninja"), tmp_path, {}) is True


@pytest.fixture
def compdb_dir(tmp_path: Path) -> Path:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    (build_dir / "build.ninja").write_text("rule x\n")
    return build_dir


def test_refresh_compile_commands_regenerates_when_stale(compdb_dir: Path) -> None:
    ninja = Path("ninja")
    compdb = compdb_dir / "compile_commands.json"
    stamp = compdb_dir / ".compile_commands.stamp"
    with patch.object(toolchain, "_write_compile_commands") as write:
        # A rewritten manifest always regenerates
        toolchain._refresh_compile_commands(ninja, compdb_dir, {}, True)
        assert write.call_count == 1
        assert stamp.is_file()
        # No compile DB yet
        toolchain._refresh_compile_commands(ninja, compdb_dir, {}, False)
        assert write.call_count == 2
        compdb.write_text("[]")
        # Fresh stamp: nothing to do
        toolchain._refresh_compile_commands(ninja, compdb_dir, {}, False)
        assert write.call_count == 2
        # A manifest newer than the stamp (interrupted previous run)
        os.utime(stamp, (1, 1))
        toolchain._refresh_compile_commands(ninja, compdb_dir, {}, False)
        assert write.call_count == 3
        # A missing stamp regenerates too
        stamp.unlink()
        toolchain._refresh_compile_commands(ninja, compdb_dir, {}, False)
        assert write.call_count == 4


def test_write_compile_commands_success(compdb_dir: Path) -> None:
    entries = json.dumps([{"file": "a.cpp", "command": "g++ -c a.cpp"}])
    with patch("subprocess.run", return_value=_completed(stdout=entries)) as run:
        toolchain._write_compile_commands(Path("ninja"), compdb_dir, {"A": "1"})
    assert (compdb_dir / "compile_commands.json").read_text() == entries
    assert run.call_args.args[0] == [
        "ninja",
        "-C",
        str(compdb_dir),
        "-t",
        "compdb",
        *toolchain.COMPILE_RULES,
    ]
    assert run.call_args.kwargs["env"] == {"A": "1"}


@pytest.mark.parametrize(
    ("result", "message"),
    [
        (_completed(rc=1, stderr="boom"), "Could not generate compile_commands.json"),
        (_completed(stdout="not json"), "unparsable compile database"),
        (_completed(stdout="[]"), "empty compile database"),
    ],
)
def test_write_compile_commands_failures_drop_stale_db(
    compdb_dir: Path, result: SimpleNamespace, message: str
) -> None:
    compdb = compdb_dir / "compile_commands.json"
    compdb.write_text("[stale]")
    with (
        patch("subprocess.run", return_value=result),
        pytest.raises(EsphomeError, match=message),
    ):
        toolchain._write_compile_commands(Path("ninja"), compdb_dir, {})
    assert not compdb.exists()


def test_get_idedata_resolves_ccache_by_default(tmp_path: Path) -> None:
    with (
        patch.object(toolchain, "resolve_ccache_path", return_value="/usr/bin/ccache"),
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata", return_value={"x": 1}
        ) as load,
    ):
        assert toolchain.get_idedata() == {"x": 1}
    load.assert_called_once_with(
        tmp_path / ".pioenvs" / "dev" / "compile_commands.json",
        tmp_path / ".pioenvs" / "dev" / "program",
        CORE.relative_internal_path("idedata", "dev.json"),
        launcher="/usr/bin/ccache",
    )


def test_get_idedata_explicit_ccache_disabled() -> None:
    with (
        patch.object(toolchain, "resolve_ccache_path") as resolve,
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata", return_value=None
        ) as load,
    ):
        assert toolchain.get_idedata(None) is None
    resolve.assert_not_called()
    assert load.call_args.kwargs["launcher"] is None


def test_run_compile_uses_real_subprocess_signature(compile_env) -> None:
    """The build call inherits stdio and never captures (progress must stream)."""
    (compile_env.build_dir / "program").write_text("")
    compile_env.run.return_value = _completed()
    toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False)
    kwargs = compile_env.run.call_args.kwargs
    assert kwargs["check"] is False
    assert kwargs["close_fds"] is False
    assert "capture_output" not in kwargs

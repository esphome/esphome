"""Tests for esphome.arduino8266.toolchain (the ninja build driver)."""

from __future__ import annotations

import os
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.arduino8266 import framework, toolchain
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE, EsphomeError

_SIZE_OUTPUT = """\
firmware.elf  :
section       size    addr
.data         1924    1073643520
.noinit       56      1073645444
.text         496     1074790400
.irom0.text   342804  1075843088
.text1        27489   1074790896
.rodata       2588    1073645504
.bss          26504   1073648096
Total         401861
"""


@pytest.fixture(autouse=True)
def _setup_core(tmp_path: Path) -> None:
    CORE.name = "test8266"
    CORE.config_path = tmp_path / "test8266.yaml"
    CORE.build_path = tmp_path
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(3, 1, 2)}
    # run_compile verifies the produced artifacts; give every test a build
    # that "produced" them (tests for the guard delete them again)
    build_dir = CORE.relative_pioenvs_path("test8266")
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "firmware.elf").write_bytes(b"")
    (build_dir / "firmware.bin").write_bytes(b"")


def _paths(tmp_path: Path) -> framework.InstalledPaths:
    return framework.InstalledPaths(
        framework=tmp_path / "framework",
        toolchain=tmp_path / "toolchain",
        ninja=tmp_path / "ninja",
    )


def test_path_getters(tmp_path: Path) -> None:
    assert toolchain.get_build_dir() == CORE.relative_pioenvs_path("test8266")
    assert toolchain.get_elf_path().name == "firmware.elf"
    # The framework accessor owns the layout and the Windows suffix
    suffix = ".exe" if os.name == "nt" else ""
    assert toolchain.get_addr2line_path().name == f"xtensa-lx106-elf-addr2line{suffix}"
    assert toolchain.get_objdump_path().name == f"xtensa-lx106-elf-objdump{suffix}"
    assert toolchain.get_readelf_path().name == f"xtensa-lx106-elf-readelf{suffix}"


def test_run_compile_build_failure(tmp_path: Path) -> None:
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        patch("esphome.build_gen.arduino8266.write_project"),
        patch.object(
            toolchain.subprocess, "run", return_value=MagicMock(returncode=2)
        ) as mock_run,
        patch.object(toolchain, "_write_compile_commands") as mock_compdb,
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=True) == 2
    cmd = mock_run.call_args[0][0]
    assert "-v" in cmd
    # The compile database is generated before the build runs, so a failed
    # build cannot leave a stale database behind.
    mock_compdb.assert_called_once()


def test_run_compile_success(tmp_path: Path) -> None:
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        # An unchanged manifest is what makes the -n probe run
        patch("esphome.build_gen.arduino8266.write_project", return_value=False),
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout="", stderr=""),
        ) as mock_run,
        patch.object(toolchain, "_write_compile_commands") as mock_compdb,
        patch.object(toolchain, "_print_size_summary") as mock_size,
        patch.object(toolchain, "get_idedata") as mock_idedata,
    ):
        rc = toolchain.run_compile(
            {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 4}}, verbose=False
        )
    assert rc == 0
    # The -n probe runs first, then the real build (cwd, no -C banner)
    ninja_calls = [c for c in mock_run.call_args_list if "ninja" in str(c[0][0][0])]
    assert ninja_calls[0][0][0][-1] == "-n"
    cmd = ninja_calls[1][0][0]
    assert cmd[-2:] == ["-j", "4"]
    assert "-C" not in cmd
    assert ninja_calls[1][1]["cwd"] is not None
    mock_compdb.assert_called_once()
    mock_size.assert_called_once()
    mock_idedata.assert_called_once()


def test_run_compile_noop_skips_the_build_spawn(tmp_path: Path) -> None:
    """A no-op rebuild stays quiet: the -n probe answers "no work to do"
    and the real ninja spawn (and its banner) never happens."""
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        # An unchanged manifest is what makes the -n probe run
        patch("esphome.build_gen.arduino8266.write_project", return_value=False),
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(
                returncode=0, stdout="ninja: no work to do.\n", stderr=""
            ),
        ) as mock_run,
        patch.object(toolchain, "_write_compile_commands"),
        patch.object(toolchain, "_print_size_summary"),
        patch.object(toolchain, "get_idedata"),
    ):
        rc = toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False)
    assert rc == 0
    ninja_calls = [c for c in mock_run.call_args_list if "ninja" in str(c[0][0][0])]
    assert len(ninja_calls) == 1
    assert ninja_calls[0][0][0][-1] == "-n"


def test_run_compile_surfaces_probe_diagnostics(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A load-time ninja diagnostic (a generator bug signal) reaches the
    user even when the no-work branch skips the real spawn."""
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        # An unchanged manifest is what makes the -n probe run
        patch("esphome.build_gen.arduino8266.write_project", return_value=False),
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(
                returncode=0,
                stdout="ninja: no work to do.\n",
                stderr="ninja: warning: multiple rules generate x\n",
            ),
        ),
        patch.object(toolchain, "_write_compile_commands"),
        patch.object(toolchain, "_print_size_summary"),
        patch.object(toolchain, "get_idedata"),
    ):
        rc = toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False)
    assert rc == 0
    assert "multiple rules generate x" in caplog.text


def test_run_compile_missing_artifact_fails(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A zero ninja exit that produced no firmware must not be a green
    build (size summary and idedata only warn)."""
    (toolchain.get_build_dir() / "firmware.elf").unlink()
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        patch("esphome.build_gen.arduino8266.write_project"),
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout="", stderr=""),
        ),
        patch.object(toolchain, "_write_compile_commands"),
        patch.object(toolchain, "_print_size_summary") as mock_size,
        patch.object(toolchain, "get_idedata"),
    ):
        rc = toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False)
    assert rc == 1
    assert "Build produced no" in caplog.text
    mock_size.assert_not_called()


def test_run_compile_warns_when_idedata_fails(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed idedata generation right after a successful build is visible,
    not deferred to a misleading error in a later command."""
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        patch("esphome.build_gen.arduino8266.write_project"),
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout="", stderr=""),
        ),
        patch.object(toolchain, "_write_compile_commands"),
        patch.object(toolchain, "_print_size_summary"),
        patch.object(toolchain, "get_idedata", return_value=None),
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert "Could not generate idedata" in caplog.text


def test_write_compile_commands(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    entries = '[{"file": "a.cpp", "command": "cc"}]\n'
    with patch.object(
        toolchain.subprocess,
        "run",
        return_value=MagicMock(returncode=0, stdout=entries),
    ):
        toolchain._write_compile_commands(tmp_path / "ninja", build_dir, {})
    assert (build_dir / "compile_commands.json").read_text() == entries


@pytest.mark.parametrize(
    ("stdout", "match"),
    [
        ("[]\n", "empty compile database"),
        # A parse failure names its cause, not the rule-name story
        ("not json", "unparsable compile database.*not json"),
    ],
)
def test_write_compile_commands_bad_db_raises(
    tmp_path: Path, stdout: str, match: str
) -> None:
    """An empty or unparsable compile database fails the build with its
    actual cause and drops any stale database."""
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    (build_dir / "compile_commands.json").write_text("[stale]")
    with (
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout=stdout),
        ),
        pytest.raises(EsphomeError, match=match),
    ):
        toolchain._write_compile_commands(tmp_path / "ninja", build_dir, {})
    assert not (build_dir / "compile_commands.json").exists()


def test_write_compile_commands_failure_removes_stale_db(tmp_path: Path) -> None:
    """A failed compdb run must not leave a stale database behind."""
    stale = tmp_path / "compile_commands.json"
    stale.write_text("[]")
    with (
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=1, stderr="boom"),
        ),
        pytest.raises(EsphomeError, match="compile_commands"),
    ):
        toolchain._write_compile_commands(tmp_path / "ninja", tmp_path, {})
    assert not stale.exists()


def test_parse_app_size(tmp_path: Path) -> None:
    ld = tmp_path / "eagle.flash.4m.ld"
    ld.write_text("MEMORY\n{\n  irom0_0_seg :  org = 0x40201010, len = 0xfeff0\n}\n")
    with patch("esphome.build_gen.arduino8266.get_flash_ld_path", return_value=ld):
        assert toolchain._parse_app_size(tmp_path, _paths(tmp_path)) == 0xFEFF0

    ld.write_text("MEMORY { }\n")
    with patch("esphome.build_gen.arduino8266.get_flash_ld_path", return_value=ld):
        assert toolchain._parse_app_size(tmp_path, _paths(tmp_path)) is None

    # A zero-length segment is bad data, not a budget; warn and drop it
    ld.write_text("MEMORY\n{\n  irom0_0_seg :  org = 0x40201010, len = 0x0\n}\n")
    with patch("esphome.build_gen.arduino8266.get_flash_ld_path", return_value=ld):
        assert toolchain._parse_app_size(tmp_path, _paths(tmp_path)) is None

    with patch(
        "esphome.build_gen.arduino8266.get_flash_ld_path",
        return_value=tmp_path / "missing.ld",
    ):
        assert toolchain._parse_app_size(tmp_path, _paths(tmp_path)) is None


def test_print_size_summary(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    with (
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout=_SIZE_OUTPUT),
        ),
        patch.object(toolchain, "_parse_app_size", return_value=1044464),
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    out = capsys.readouterr().out
    # Exact PlatformIO shape so script/ci_memory_impact_extract.py can parse it
    assert "RAM:   [====      ]  37.9% (used 31016 bytes from 81920 bytes)" in out
    assert "Flash: [====      ]  35.9% (used 375301 bytes from 1044464 bytes)" in out


def test_print_size_summary_missing_size_tool_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A missing size binary degrades to a warning; the firmware already
    linked and must not be discarded."""
    with patch.object(
        toolchain.subprocess, "run", side_effect=FileNotFoundError("no size")
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    assert "Could not summarize firmware size" in caplog.text


def test_print_size_summary_no_app_size(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    with (
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout=_SIZE_OUTPUT),
        ),
        patch.object(toolchain, "_parse_app_size", return_value=None),
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    out = capsys.readouterr().out
    # Both lines are skipped together: a RAM line without Flash would skew
    # CI's memory-impact sums across builds
    assert out == ""


def test_print_size_summary_size_tool_failure(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    with patch.object(
        toolchain.subprocess,
        "run",
        return_value=MagicMock(returncode=1, stdout="", stderr="bad elf"),
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    assert capsys.readouterr().out == ""
    assert "Could not summarize firmware size" in caplog.text


def test_get_idedata_delegates(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata",
            return_value={"cc_path": "x"},
        ) as mock_load,
        patch.object(toolchain, "resolve_ccache_path", return_value="/cc/ccache"),
    ):
        assert toolchain.get_idedata() == {"cc_path": "x"}
    compile_commands, elf, cache = mock_load.call_args[0]
    assert compile_commands.name == "compile_commands.json"
    assert elf.name == "firmware.elf"
    assert cache.name == "test8266.arduino.json"
    # The exact configured launcher string is passed for compile DB parsing
    # (resolve_ccache_path returns a str, untouched on every platform)
    assert mock_load.call_args.kwargs["launcher"] == "/cc/ccache"


def test_get_idedata_no_ccache(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata", return_value={}
        ) as mock_load,
        patch.object(toolchain, "resolve_ccache_path", return_value=None),
    ):
        toolchain.get_idedata()
    assert mock_load.call_args.kwargs["launcher"] is None


def test_run_compile_skips_compdb_when_ninja_unchanged(tmp_path: Path) -> None:
    """An unchanged build.ninja means the compile DB is already current."""
    build_dir = toolchain.get_build_dir()
    build_dir.mkdir(parents=True, exist_ok=True)

    def run(regenerate_expected: bool) -> None:
        with (
            patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
            patch.object(framework, "get_build_env", return_value={}),
            patch("esphome.build_gen.arduino8266.write_project", return_value=False),
            patch.object(
                toolchain.subprocess,
                "run",
                return_value=MagicMock(returncode=0, stdout="", stderr=""),
            ),
            patch.object(toolchain, "_write_compile_commands") as mock_compdb,
            patch.object(toolchain, "_print_size_summary"),
            patch.object(toolchain, "get_idedata"),
        ):
            assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
        assert mock_compdb.called == regenerate_expected

    # Missing compile DB: regenerated even though build.ninja is unchanged
    run(regenerate_expected=True)
    # Present compile DB + unchanged build.ninja: skipped
    (build_dir / "compile_commands.json").write_text("[]")
    run(regenerate_expected=False)


def test_print_size_summary_unparsable_section(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A totals-relevant section that fails to parse must not produce a
    confident wrong number; an irrelevant one only warns."""
    bad = _SIZE_OUTPUT.replace(".bss          26504", ".bss          abc")
    with patch.object(
        toolchain.subprocess,
        "run",
        return_value=MagicMock(returncode=0, stdout=bad),
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    assert capsys.readouterr().out == ""
    assert "Unparsable size output" in caplog.text

    caplog.clear()
    harmless = _SIZE_OUTPUT + ".broken   abc   0\n"
    with (
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout=harmless),
        ),
        patch.object(toolchain, "_parse_app_size", return_value=1044464),
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    assert "RAM:" in capsys.readouterr().out
    assert "Unparsable size output" in caplog.text


def test_print_size_summary_missing_section_skips_summary(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A totals section absent from the output must not default to zero."""
    without_bss = "\n".join(
        line for line in _SIZE_OUTPUT.splitlines() if ".bss" not in line
    )
    with patch.object(
        toolchain.subprocess,
        "run",
        return_value=MagicMock(returncode=0, stdout=without_bss),
    ):
        toolchain._print_size_summary(tmp_path, _paths(tmp_path))
    assert capsys.readouterr().out == ""
    assert "missing section(s) .bss" in caplog.text


def test_warn_ignored_platformio_options(caplog: pytest.LogCaptureFixture) -> None:
    """Component-added options the native build drops are warned by name;
    the honored ones (lib_ignore, f_cpu, ldscript) stay quiet."""
    CORE.platformio_options = {
        "board_build.ldscript": "eagle.flash.4m2m.ld",
        "board_build.f_cpu": "160000000L",
        "board_build.filesystem": "littlefs",
        "lib_ignore": ["Updater"],
        "upload_speed": "460800",
    }
    toolchain._warn_ignored_platformio_options()
    assert "platformio_options->board_build.filesystem is ignored" in caplog.text
    assert "native 'arduino' toolchain" in caplog.text
    assert "board_build.ldscript is ignored" not in caplog.text
    assert "board_build.f_cpu is ignored" not in caplog.text
    assert "lib_ignore" not in caplog.text
    # Component-added upload_speed never gets read under the native
    # toolchain, so it must warn
    assert "upload_speed" in caplog.text


def test_run_compile_idedata_error_does_not_fail_build(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unusable compile DB after a successful build warns, never fails."""
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        patch("esphome.build_gen.arduino8266.write_project"),
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout="", stderr=""),
        ),
        patch.object(toolchain, "_write_compile_commands"),
        patch.object(toolchain, "_print_size_summary"),
        patch.object(
            toolchain,
            "get_idedata",
            side_effect=EsphomeError("compile database is unusable"),
        ),
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 0
    assert "Could not generate idedata: compile database is unusable" in caplog.text


def test_get_idedata_accepts_preresolved_ccache() -> None:
    """run_compile threads its resolved ccache through; the probe must not
    run again."""
    with (
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata",
            return_value={"ok": True},
        ) as mock_build,
        patch.object(toolchain, "resolve_ccache_path") as mock_resolve,
    ):
        assert toolchain.get_idedata("/usr/bin/ccache") == {"ok": True}
    mock_resolve.assert_not_called()
    assert mock_build.call_args.kwargs["launcher"] == "/usr/bin/ccache"

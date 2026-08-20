"""Tests for esphome.arduino8266.toolchain (the ninja build driver)."""

from __future__ import annotations

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
from esphome.core import CORE

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
.comment      abc     0
Total         401861
"""


@pytest.fixture(autouse=True)
def _setup_core(tmp_path: Path) -> None:
    CORE.name = "test8266"
    CORE.config_path = tmp_path / "test8266.yaml"
    CORE.build_path = tmp_path
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(3, 1, 2)}


def _paths(tmp_path: Path) -> dict[str, Path]:
    return {
        "framework_path": tmp_path / "framework",
        "toolchain_path": tmp_path / "toolchain",
        "ninja_path": tmp_path / "ninja",
    }


def test_path_getters(tmp_path: Path) -> None:
    assert toolchain.get_build_dir() == CORE.relative_pioenvs_path("test8266")
    assert toolchain.get_elf_path().name == "firmware.elf"
    assert toolchain.get_addr2line_path().name == "xtensa-lx106-elf-addr2line"


def test_run_compile_build_failure(tmp_path: Path) -> None:
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        patch("esphome.build_gen.arduino8266.write_project"),
        patch.object(
            toolchain.subprocess, "run", return_value=MagicMock(returncode=2)
        ) as mock_run,
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=True) == 2
    cmd = mock_run.call_args[0][0]
    assert "-v" in cmd


def test_run_compile_success(tmp_path: Path) -> None:
    with (
        patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
        patch.object(framework, "get_build_env", return_value={}),
        patch("esphome.build_gen.arduino8266.write_project"),
        patch.object(
            toolchain.subprocess, "run", return_value=MagicMock(returncode=0)
        ) as mock_run,
        patch.object(toolchain, "_write_compile_commands") as mock_compdb,
        patch.object(toolchain, "_print_size_summary") as mock_size,
        patch.object(toolchain, "get_idedata") as mock_idedata,
    ):
        rc = toolchain.run_compile(
            {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 4}}, verbose=False
        )
    assert rc == 0
    cmd = mock_run.call_args[0][0]
    assert cmd[-2:] == ["-j", "4"]
    mock_compdb.assert_called_once()
    mock_size.assert_called_once()
    mock_idedata.assert_called_once()


def test_write_compile_commands(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    with patch.object(
        toolchain.subprocess,
        "run",
        return_value=MagicMock(returncode=0, stdout="[]\n"),
    ):
        toolchain._write_compile_commands(tmp_path / "ninja", build_dir, {})
    assert (build_dir / "compile_commands.json").read_text() == "[]\n"


def test_write_compile_commands_failure(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    with patch.object(
        toolchain.subprocess,
        "run",
        return_value=MagicMock(returncode=1, stderr="boom"),
    ):
        toolchain._write_compile_commands(tmp_path / "ninja", tmp_path, {})
    assert "Could not generate compile_commands.json" in caplog.text


def test_parse_app_size(tmp_path: Path) -> None:
    ld = tmp_path / "eagle.flash.4m.ld"
    ld.write_text("MEMORY\n{\n  irom0_0_seg :  org = 0x40201010, len = 0xfeff0\n}\n")
    with patch("esphome.build_gen.arduino8266.get_flash_ld_path", return_value=ld):
        assert toolchain._parse_app_size(tmp_path) == 0xFEFF0

    ld.write_text("MEMORY { }\n")
    with patch("esphome.build_gen.arduino8266.get_flash_ld_path", return_value=ld):
        assert toolchain._parse_app_size(tmp_path) is None

    with patch(
        "esphome.build_gen.arduino8266.get_flash_ld_path",
        return_value=tmp_path / "missing.ld",
    ):
        assert toolchain._parse_app_size(tmp_path) is None


def test_print_size_summary(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    with (
        patch.object(
            toolchain.subprocess,
            "run",
            return_value=MagicMock(returncode=0, stdout=_SIZE_OUTPUT),
        ),
        patch.object(toolchain, "_parse_app_size", return_value=1044464),
    ):
        toolchain._print_size_summary(tmp_path, tmp_path / "toolchain")
    out = capsys.readouterr().out
    # Exact PlatformIO shape so script/ci_memory_impact_extract.py can parse it
    assert "RAM:   [====      ]  37.9% (used 31016 bytes from 81920 bytes)" in out
    assert "Flash: [====      ]  35.9% (used 375301 bytes from 1044464 bytes)" in out


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
        toolchain._print_size_summary(tmp_path, tmp_path / "toolchain")
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "Flash:" not in out


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
        toolchain._print_size_summary(tmp_path, tmp_path / "toolchain")
    assert capsys.readouterr().out == ""
    assert "Could not summarize firmware size" in caplog.text


def test_get_idedata_delegates(tmp_path: Path) -> None:
    with patch(
        "esphome.espidf.idedata.load_or_build_idedata", return_value={"cc_path": "x"}
    ) as mock_load:
        assert toolchain.get_idedata() == {"cc_path": "x"}
    compile_commands, elf, cache = mock_load.call_args[0]
    assert compile_commands.name == "compile_commands.json"
    assert elf.name == "firmware.elf"
    assert cache.name == "test8266.json"


def test_run_compile_skips_compdb_when_ninja_unchanged(tmp_path: Path) -> None:
    """An unchanged build.ninja means the compile DB is already current."""
    build_dir = toolchain.get_build_dir()
    build_dir.mkdir(parents=True)

    def run(regenerate_expected: bool) -> None:
        with (
            patch.object(framework, "check_and_install", return_value=_paths(tmp_path)),
            patch.object(framework, "get_build_env", return_value={}),
            patch("esphome.build_gen.arduino8266.write_project", return_value=False),
            patch.object(
                toolchain.subprocess, "run", return_value=MagicMock(returncode=0)
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

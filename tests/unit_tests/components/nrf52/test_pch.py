"""nrf52 sdk-nrf precompiled header: the CMake block and the ccache checksum."""

from pathlib import Path
from unittest.mock import Mock, patch

import pytest

from esphome.components import nrf52
from esphome.components.zephyr.const import KEY_BOARD
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE, EsphomeError

SUM = "CMakeFiles/app.dir/cmake_pch.hxx.gch.sum"


def _generate_cmake(tmp_path: Path) -> str:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    with (
        patch(
            "esphome.components.zephyr.library.generate_zephyr_modules",
            return_value=[],
        ),
        patch.object(nrf52, "get_project_compile_flags", return_value=["-Os"]),
        patch.object(nrf52, "get_project_link_flags", return_value=[]),
    ):
        nrf52._generate_cmake_lists()
    return (tmp_path / "build" / "zephyr" / "CMakeLists.txt").read_text()


def test_cmake_lists_precompile_the_core_headers(tmp_path: Path) -> None:
    text = _generate_cmake(tmp_path)
    assert "target_precompile_headers(app PRIVATE ${esphome_pch_headers})" in text
    assert "/../src/esphome/core/pch_prefix.h" in text
    # The headers Zephyr forces leave the C++ compiles of the app only
    assert (
        "$<$<NOT:$<AND:$<COMPILE_LANGUAGE:CXX>,"
        "$<STREQUAL:$<TARGET_PROPERTY:NAME>,app>>>:${option}>"
    ) in text
    assert r'REPLACE "(.+)" "$<$<COMPILE_LANGUAGE:CXX>:\\1>"' in text


def test_cmake_lists_pch_block_disabled(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    text = _generate_cmake(tmp_path)
    assert "precompile" not in text
    assert "zephyr_interface" not in text


def _write_checksum(tmp_path: Path, app: str, conf: str = "CONFIG_X=y\n") -> Path:
    """Write the checksum for a build dir whose app image sits in ``app``."""
    CORE.build_path = tmp_path
    header = tmp_path / "src" / "esphome" / "core" / "pch_prefix.h"
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text("#define M 1\n")
    source_dir = tmp_path / "zephyr"
    source_dir.mkdir(exist_ok=True)
    (source_dir / "prj.conf").write_text(conf)
    (source_dir / "CMakeLists.txt").write_text("not part of the checksum\n")
    build_dir = tmp_path / ".pioenvs" / "livingroom"
    (build_dir / app).mkdir(parents=True, exist_ok=True)
    (build_dir / app / "CMakeCache.txt").write_text("")
    with (
        patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: "2.9.2"}}),
        patch.object(nrf52, "zephyr_data", return_value={KEY_BOARD: "board"}),
    ):
        nrf52._write_pch_checksum(build_dir, source_dir)
    return build_dir / app / SUM


@pytest.mark.parametrize("app", ["zephyr", "."])
def test_pch_checksum_is_written_next_to_the_gch(tmp_path: Path, app: str) -> None:
    """Sysbuild nests the app image; without it the build dir is the app."""
    sum_path = _write_checksum(tmp_path, app)
    assert len(sum_path.read_text().strip()) == 64


def test_pch_checksum_tracks_the_zephyr_configuration(tmp_path: Path) -> None:
    first = _write_checksum(tmp_path, "zephyr").read_text()
    assert _write_checksum(tmp_path, "zephyr", "CONFIG_X=n\n").read_text() != first


def test_pch_checksum_waits_for_the_first_configure(tmp_path: Path) -> None:
    CORE.build_path = tmp_path
    build_dir = tmp_path / ".pioenvs" / "livingroom"
    nrf52._write_pch_checksum(build_dir, tmp_path / "zephyr")
    assert not build_dir.exists()


@pytest.fixture
def run_cmd(tmp_path: Path) -> Mock:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    CORE.toolchain = Toolchain.SDK_NRF
    with (
        patch.object(nrf52, "check_and_install"),
        patch.object(nrf52, "_generate_cmake_lists", return_value=False),
        patch.object(
            nrf52,
            "get_build_paths",
            return_value={"python_executable": "python3", "framework_path": tmp_path},
        ),
        patch.object(nrf52, "get_build_env", return_value={}),
        patch.object(nrf52, "zephyr_data", return_value={KEY_BOARD: "board"}),
        patch.object(nrf52, "run_command_ok", return_value=False) as run,
    ):
        yield run


def test_ccache_pch_settings_reach_west(run_cmd: Mock) -> None:
    # clear=True also drops ambient CCACHE_* and ESPHOME_PCH_* overrides
    with (
        patch.dict("os.environ", {}, clear=True),
        pytest.raises(EsphomeError, match="nRF52 native build failed"),
    ):
        nrf52.run_compile(None, {})
    env = run_cmd.call_args.kwargs["env"]
    assert env["CCACHE_PCH_EXTSUM"] == "true"
    assert env["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"


def test_disabled_leaves_the_west_environment_alone(
    run_cmd: Mock, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    with pytest.raises(EsphomeError, match="nRF52 native build failed"):
        nrf52.run_compile(None, {})
    assert "CCACHE_PCH_EXTSUM" not in run_cmd.call_args.kwargs["env"]

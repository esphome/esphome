"""nrf52 sdk-nrf precompiled header: the CMake block and the ccache checksum."""

from pathlib import Path
from unittest.mock import Mock, patch

import pytest

from esphome.build_helpers import pch
from esphome.components import nrf52
from esphome.components.nrf52 import framework
from esphome.components.nrf52.toolchain import get_elf_path
from esphome.components.zephyr.const import KEY_BOARD, KEY_SYSBUILD
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE, EsphomeError

SUM = "CMakeFiles/app.dir/cmake_pch.hxx.gch.sum"


def _generate_cmake(tmp_path: Path, pch_on: bool = True) -> str:
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
        nrf52._generate_cmake_lists(pch_on)
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
    # A Zephyr that spells them another way must not go unnoticed
    assert "if(NOT esphome_pch_headers)\n  message(FATAL_ERROR" in text


def test_cmake_lists_pch_block_disabled(tmp_path: Path) -> None:
    text = _generate_cmake(tmp_path, pch_on=False)
    assert "precompile" not in text
    assert "zephyr_interface" not in text


@pytest.mark.parametrize(("version", "on"), [((12, 2, 0), False), ((14, 4, 0), True)])
def test_the_zephyr_compiler_decides_on_windows(
    windows_gcc_rule: None, version: tuple[int, ...], on: bool
) -> None:
    # platformdirs would pick its Windows backend from the patched sys.platform
    with (
        patch.object(nrf52, "toolchain_tool", lambda name: Path(f"/sdk/{name}.exe")),
        patch.object(pch, "gcc_version", return_value=version) as asked,
    ):
        assert nrf52._pch_usable() is on
    assert asked.call_args.args[0] == (Path("/sdk/g++.exe"),)


def _write_checksum(
    tmp_path: Path,
    app: str,
    conf: str = "CONFIG_X=y\n",
    configured: bool = True,
    sysbuild: bool = False,
    version: cv.Version | None = None,
) -> Path:
    """Write the checksum for a build dir whose app image sits in ``app``."""
    version = version or cv.Version(2, 9, 2)
    CORE.build_path = tmp_path
    header = tmp_path / "src" / "esphome" / "core" / "pch_prefix.h"
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text("#define M 1\n")
    source_dir = tmp_path / "zephyr"
    source_dir.mkdir(exist_ok=True)
    (source_dir / "prj.conf").write_text(conf)
    (source_dir / "CMakeLists.txt").write_text("not part of the checksum\n")
    build_dir = tmp_path / ".pioenvs" / "livingroom"
    if configured:
        (build_dir / app).mkdir(parents=True, exist_ok=True)
        (build_dir / app / "CMakeCache.txt").write_text("")
    with (
        patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: version}}),
        patch.object(
            nrf52,
            "zephyr_data",
            return_value={KEY_BOARD: "board", KEY_SYSBUILD: sysbuild},
        ),
    ):
        nrf52._write_pch_checksum(build_dir, source_dir)
    return build_dir / app / SUM


@pytest.mark.parametrize(
    ("app", "version"), [("zephyr", cv.Version(2, 9, 2)), (".", cv.Version(2, 9, 1))]
)
def test_pch_checksum_is_written_next_to_the_gch(
    tmp_path: Path, app: str, version: cv.Version
) -> None:
    """The SDK version decides the layout, like get_elf_path."""
    sum_path = _write_checksum(tmp_path, app, version=version)
    assert len(sum_path.read_text().strip()) == 64


def test_pch_checksum_tracks_the_kconfig_side_inputs(tmp_path: Path) -> None:
    """West projects and the sysbuild flag reach autoconf without a .conf
    line; the sum must move with them or stale objects get served."""
    first = _write_checksum(tmp_path, "zephyr").read_text()
    with patch.object(nrf52, "wanted_west_projects", return_value={"extra"}):
        second = _write_checksum(tmp_path, "zephyr").read_text()
    assert first != second
    third = _write_checksum(tmp_path, "zephyr", sysbuild=True).read_text()
    assert first != third


def test_pch_checksum_tracks_the_zephyr_configuration(tmp_path: Path) -> None:
    first = _write_checksum(tmp_path, "zephyr").read_text()
    assert _write_checksum(tmp_path, "zephyr", "CONFIG_X=n\n").read_text() != first


@pytest.mark.parametrize("version", [cv.Version(2, 9, 2), cv.Version(2, 9, 1)])
def test_pch_checksum_lands_where_the_build_writes_the_image(
    tmp_path: Path, version: cv.Version
) -> None:
    """One layout rule: the sum must sit in get_elf_path's app dir, or a
    layout drift silently costs the first build's sharing."""
    app = "zephyr" if version >= cv.Version(2, 9, 2) else "."
    sum_path = _write_checksum(tmp_path, app, version=version)
    CORE.name = "livingroom"
    with patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: version}}):
        expected = get_elf_path().parent.parent / SUM
    assert sum_path.resolve() == expected.resolve()


def test_pch_checksum_written_before_the_first_configure(tmp_path: Path) -> None:
    """The first build's compiles hash the sum in place of the .gch."""
    assert _write_checksum(tmp_path, "zephyr", configured=False).is_file()


def _fake_build_env(ccache: str | None) -> dict[str, str]:
    """The real get_build_env with only the install path lookups stubbed."""
    with (
        patch.object(framework, "_get_version_str", return_value="v1"),
        patch.object(framework, "_get_python_env_path", return_value=Path("/penv")),
        patch.object(
            framework,
            "get_python_env_executable_path",
            return_value=Path("/penv/bin/python"),
        ),
        patch.object(framework, "_get_framework_path", return_value=Path("/fw")),
        patch.object(framework, "_get_toolchain_version", return_value="t1"),
        patch.object(framework, "_get_toolchain_path", return_value=Path("/tc")),
    ):
        return framework.get_build_env(ccache)


@pytest.fixture
def run_cmd(tmp_path: Path) -> Mock:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    CORE.toolchain = Toolchain.SDK_NRF
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(3, 2, 0)}
    (tmp_path / "build" / "zephyr").mkdir(parents=True)
    with (
        patch.dict("os.environ", {}, clear=True),
        patch.object(nrf52, "check_and_install"),
        patch.object(nrf52, "_generate_cmake_lists", return_value=False),
        patch.object(
            nrf52,
            "get_build_paths",
            return_value={"python_executable": "python3", "framework_path": tmp_path},
        ),
        patch.object(nrf52, "get_build_env", side_effect=_fake_build_env),
        patch.object(nrf52, "resolve_ccache_path", return_value="/usr/bin/ccache"),
        patch.object(nrf52, "zephyr_data", return_value={KEY_BOARD: "board"}),
        patch.object(nrf52, "run_command_ok", return_value=False) as run,
    ):
        yield run


def test_shared_ccache_settings_reach_west(run_cmd: Mock, tmp_path: Path) -> None:
    # The header is on explicitly since Windows hosts start with it off
    with (
        patch.dict("os.environ", {"ESPHOME_PCH_ENABLE": "1"}),
        pytest.raises(EsphomeError, match="nRF52 native build failed"),
    ):
        nrf52.run_compile(None, {})
    env = run_cmd.call_args.kwargs["env"]
    assert env["CCACHE_PCH_EXTSUM"] == "true"
    assert env["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    # Without depend mode a Kconfig flip reuses a stale .gch
    assert env["CCACHE_DEPEND"] == "1"
    # The full managed set, not a bespoke subset
    assert env["CCACHE_DIR"].endswith("ccache")
    assert env["CCACHE_NOHASHDIR"] == "true"
    assert env["CCACHE_BASEDIR"] == str((tmp_path / "build").resolve())
    assert "CCACHE_DISABLE" not in env


def test_user_exported_ccache_values_win(run_cmd: Mock) -> None:
    user = {"ESPHOME_PCH_ENABLE": "1", "CCACHE_DEPEND": "0", "CCACHE_DIR": "/mine"}
    with (
        patch.dict("os.environ", user),
        pytest.raises(EsphomeError, match="nRF52 native build failed"),
    ):
        nrf52.run_compile(None, {})
    env = run_cmd.call_args.kwargs["env"]
    assert env["CCACHE_DEPEND"] == "0"
    assert env["CCACHE_DIR"] == "/mine"


def test_no_ccache_disables_the_zephyr_launcher(run_cmd: Mock) -> None:
    """ESPHOME_CCACHE_ENABLE=0 must also stop Zephyr's self-enabled ccache."""
    with (
        patch.object(nrf52, "resolve_ccache_path", return_value=None),
        pytest.raises(EsphomeError, match="nRF52 native build failed"),
    ):
        nrf52.run_compile(None, {})
    env = run_cmd.call_args.kwargs["env"]
    assert env["CCACHE_DISABLE"] == "1"
    assert "CCACHE_DEPEND" not in env


def test_disabled_pch_still_gets_the_shared_settings(run_cmd: Mock) -> None:
    with (
        patch.dict("os.environ", {"ESPHOME_PCH_ENABLE": "0"}),
        pytest.raises(EsphomeError, match="nRF52 native build failed"),
    ):
        nrf52.run_compile(None, {})
    env = run_cmd.call_args.kwargs["env"]
    assert "CCACHE_PCH_EXTSUM" not in env
    assert env["CCACHE_DEPEND"] == "1"

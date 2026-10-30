"""The compile flags the zephyr platform adds reach the app target."""

from pathlib import Path
from unittest.mock import patch

from esphome.components import nrf52
from esphome.components.zephyr import zephyr_to_code
from esphome.core import CORE


def test_double_promotion_warning_is_off_for_the_app(tmp_path: Path) -> None:
    """Zephyr's base warnings include -Wdouble-promotion, which every float logged would trip."""
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    with patch("esphome.components.zephyr.zephyr_add_prj_conf"):
        zephyr_to_code({})
    assert "-Wno-double-promotion" in CORE.build_flags

    with (
        patch(
            "esphome.components.zephyr.library.generate_zephyr_modules",
            return_value=[],
        ),
        patch.object(nrf52, "get_cmake_link_flags", return_value=[]),
    ):
        nrf52._generate_cmake_lists(False)
    text = (tmp_path / "build" / "zephyr" / "CMakeLists.txt").read_text()
    # The define may sit in the target options, which come before the ones inherited from
    # zephyr_interface; the warning flag must not, or Zephyr's -Wdouble-promotion would win
    target_options = text.split("target_compile_options(app PRIVATE", 1)[1]
    assert '"-DUSE_ZEPHYR"' in target_options
    assert (
        "-Wno-double-promotion" not in text.split("set_source_files_properties", 1)[0]
    )
    assert (
        "set_source_files_properties(${APP_SOURCES} PROPERTIES\n"
        '  COMPILE_OPTIONS "-Wno-double-promotion"\n)'
    ) in text


def test_build_flags_are_split_and_quoted(tmp_path: Path) -> None:
    """Entries are split like the other builds do, each flag is quoted as a
    CMake argument, and the linker flags of one entry stay one group."""
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    CORE.build_flags = {
        '-DSTR=\\"hi\\" -DSP="a b" -Wno-unused',
        "-Wl,--start-group -Wl,-lfoo -Wl,--end-group",
        "-Wl,--start-group -Wl,-lbar -Wl,--end-group",
    }
    with patch(
        "esphome.components.zephyr.library.generate_zephyr_modules",
        return_value=[],
    ):
        nrf52._generate_cmake_lists(False)
    text = (tmp_path / "build" / "zephyr" / "CMakeLists.txt").read_text()
    assert (
        'target_compile_options(app PRIVATE\n  "-DSTR=\\"hi\\""\n  "-DSP=a b"\n)'
    ) in text
    assert '  COMPILE_OPTIONS "-Wno-unused"\n' in text
    assert (
        "zephyr_ld_options(\n"
        '  "-Wl,--start-group,-lbar,--end-group"\n'
        '  "-Wl,--start-group,-lfoo,--end-group"\n'
        ")"
    ) in text

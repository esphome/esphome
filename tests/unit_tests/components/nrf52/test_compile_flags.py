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
        patch.object(nrf52, "get_project_link_flags", return_value=[]),
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

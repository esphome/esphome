from collections.abc import Callable
import re

from tests.testing_helpers import get_define_value


def _list_items(main_cpp: str, upper: str) -> list[str]:
    match = re.search(rf"APP_{upper}_LIST PROGMEM = \{{(.*)\}};", main_cpp)
    assert match is not None, upper
    return re.findall(r"\((\w+)__pstorage\)", match.group(1))


def test_devices_and_areas_are_listed_in_order(
    generate_main: Callable[[str], str],
) -> None:
    main_cpp = generate_main("tests/component_tests/app_lists/devices_and_areas.yaml")
    assert _list_items(main_cpp, "AREA") == [
        "esphome__main_area",
        "esphome__second_area",
    ]
    assert _list_items(main_cpp, "DEVICE") == [
        "esphome__first_device",
        "esphome__second_device",
    ]
    assert get_define_value("ESPHOME_AREA_COUNT") == "2"
    assert get_define_value("ESPHOME_DEVICE_COUNT") == "2"
    assert 'static_assert(ESPHOME_DEVICE_COUNT == 2, "' in main_cpp
    assert 'static_assert(ESPHOME_AREA_COUNT == 2, "' in main_cpp

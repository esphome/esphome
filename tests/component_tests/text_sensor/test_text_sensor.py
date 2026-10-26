"""Tests for the text sensor component."""

from collections.abc import Callable
import re

from tests.component_tests.helpers import INTERNAL_BIT, extract_packed_value


def test_text_sensor_is_setup(generate_main):
    """
    When the text is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    assert "static template_::TemplateTextSensor *const" in main_cpp
    assert ") template_::TemplateTextSensor();" in main_cpp
    assert "App.register_text_sensor" in main_cpp


def test_text_sensor_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    for i in (1, 2, 3):
        name = f'ESPHOME_PSTR("Template Text Sensor {i}")'
        assert f"App.register_text_sensor(ts_{i}, {name}," in main_cpp


def test_text_sensor_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then: ts_2 has internal: true, ts_3 has internal: false
    assert extract_packed_value(main_cpp, "ts_2") & INTERNAL_BIT != 0
    assert extract_packed_value(main_cpp, "ts_3") & INTERNAL_BIT == 0


def test_text_sensor_device_class_set(generate_main):
    """
    When the device_class of text_sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then: ts_2 has device_class: timestamp, ts_3 has device_class: date
    # so their packed values must be non-zero
    packed_ts_2 = extract_packed_value(main_cpp, "ts_2")
    assert packed_ts_2 != 0
    packed_ts_3 = extract_packed_value(main_cpp, "ts_3")
    assert packed_ts_3 != 0


def test_substitution_filters_share_progmem_tables(
    generate_main: Callable[[str], str],
) -> None:
    """Map and substitute pairs live in PROGMEM tables; equal lists share one."""
    main_cpp = generate_main(
        "tests/component_tests/text_sensor/test_text_sensor_filter_tables.yaml"
    )

    tables = re.findall(
        r"static constexpr text_sensor::Substitution (\w+)\[\] PROGMEM", main_cpp
    )
    assert len(tables) == 2
    map_table, sub_table = tables
    assert main_cpp.count(f"text_sensor::MapFilter({map_table})") == 2
    assert f"text_sensor::SubstituteFilter({sub_table})" in main_cpp
    # Each table ends with an empty pair so the filters store no count.
    assert main_cpp.count(".from = nullptr") == 2


def test_substitution_table_names_avoid_user_ids(
    generate_main: Callable[[str], str],
) -> None:
    """A user id equal to a table name cannot collide with the generated table."""
    main_cpp = generate_main(
        "tests/component_tests/text_sensor/test_text_sensor_filter_table_id_clash.yaml"
    )

    tables = re.findall(
        r"static constexpr text_sensor::Substitution (\w+)\[\] PROGMEM", main_cpp
    )
    assert len(tables) == 2
    assert "text_sensor_substitutions" not in tables
    assert "text_sensor_mapfilter_id_table" not in tables


def test_throttle_and_timeout_filters(generate_main: Callable[[str], str]) -> None:
    """Priority values go in a PROGMEM table ended by nullptr; no priority is a plain throttle."""
    main_cpp = generate_main(
        "tests/component_tests/text_sensor/test_text_sensor_throttle_timeout.yaml"
    )

    table = re.search(
        r'static constexpr const char \* (\w+)\[\] PROGMEM = \{"NA", nullptr\}',
        main_cpp,
    )
    assert table is not None
    # timeout (last) then throttle_with_priority with the same period merge into one filter
    assert f"text_sensor::TimeoutThrottleFilter(1000, {table.group(1)})" in main_cpp
    assert "text_sensor::ThrottleWithPriorityFilter(" not in main_cpp
    assert "text_sensor::ThrottleFilter(500)" in main_cpp
    assert "text_sensor::TimeoutThrottleFilter(2000, nullptr)" in main_cpp
    assert 'text_sensor::TimeoutFilterConfigured(30000, "Offline")' in main_cpp

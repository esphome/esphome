"""Tests for the button component"""

import re

_INTERNAL_BIT = 1 << 24


def _extract_packed_value(main_cpp, var_name):
    """Extract the third (packed) argument from a configure_entity_ call."""
    pattern = rf"{re.escape(var_name)}->configure_entity_\([^,]+,\s*\w+,\s*(\d+)\)"
    match = re.search(pattern, main_cpp)
    assert match, f"configure_entity_ call not found for {var_name}"
    return int(match.group(1))


def test_button_is_setup(generate_main):
    """
    When the button is set in the yaml file if should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/button/test_button.yaml")

    # Then
    assert "new wake_on_lan::WakeOnLanButton();" in main_cpp
    assert "App.register_button" in main_cpp
    assert "App.register_component" in main_cpp


def test_button_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/button/test_button.yaml")

    # Then
    assert 'wol_1->configure_entity_("wol_test_1",' in main_cpp
    assert "wol_2->set_macaddr(18, 52, 86, 120, 144, 171);" in main_cpp


def test_button_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/button/test_button.yaml")

    # Then: wol_1 has internal: true, wol_2 has internal: false
    assert _extract_packed_value(main_cpp, "wol_1") & _INTERNAL_BIT != 0
    assert _extract_packed_value(main_cpp, "wol_2") & _INTERNAL_BIT == 0

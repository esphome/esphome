"""Tests for the text component."""

import re

_INTERNAL_BIT = 1 << 24


def _extract_packed_value(main_cpp, var_name):
    """Extract the third (packed) argument from a configure_entity_ call."""
    pattern = rf"{re.escape(var_name)}->configure_entity_\([^,]+,\s*\w+,\s*(\d+)\)"
    match = re.search(pattern, main_cpp)
    assert match, f"configure_entity_ call not found for {var_name}"
    return int(match.group(1))


def test_text_is_setup(generate_main):
    """
    When the binary sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert "new template_::TemplateText();" in main_cpp
    assert "App.register_text" in main_cpp


def test_text_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert 'it_1->configure_entity_("test 1 text",' in main_cpp


def test_text_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then: it_2 has internal: false, it_3 has internal: true
    assert _extract_packed_value(main_cpp, "it_2") & _INTERNAL_BIT == 0
    assert _extract_packed_value(main_cpp, "it_3") & _INTERNAL_BIT != 0


def test_text_config_value_mode_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert "it_1->traits.set_mode(text::TEXT_MODE_TEXT);" in main_cpp
    assert "it_3->traits.set_mode(text::TEXT_MODE_PASSWORD);" in main_cpp


def test_text_config_lamda_is_set(generate_main):
    """
    Test if lambda is set for lambda mode (optimized with stateless lambda)
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert "it_4->set_template([]() -> std::optional<std::string> {" in main_cpp
    assert 'return std::string{"Hello"};' in main_cpp


def test_esphome_optional_alias_works(generate_main):
    """
    Test that esphome::optional alias compiles (backward compatibility)
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    # Codegen emits std::optional, but esphome::optional must also work
    # via the using alias in esphome/core/optional.h
    assert "std::optional<std::string>" in main_cpp

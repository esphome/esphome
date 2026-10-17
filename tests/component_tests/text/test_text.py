"""Tests for the text component."""

from tests.component_tests.helpers import INTERNAL_BIT, extract_packed_value


def test_text_is_setup(generate_main):
    """
    When the binary sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert "static template_::TemplateText *const" in main_cpp
    assert ") template_::TemplateText();" in main_cpp
    assert "App.register_text" in main_cpp


def test_text_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert 'App.register_text(it_1, "test 1 text",' in main_cpp


def test_text_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then: it_2 has internal: false, it_3 has internal: true
    assert extract_packed_value(main_cpp, "it_2") & INTERNAL_BIT == 0
    assert extract_packed_value(main_cpp, "it_3") & INTERNAL_BIT != 0


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

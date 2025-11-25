"""Tests for the text component."""

from esphome.core import CORE


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
    assert 'it_1->set_name_and_object_id("test 1 text", "test_1_text");' in main_cpp


def test_text_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Then
    assert "it_2->set_internal(false);" in main_cpp
    assert "it_3->set_internal(true);" in main_cpp


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


def test_text_config_lambda_is_set(generate_main) -> None:
    """
    Test if lambda is set for lambda mode (optimized with stateless lambda and deduplication)
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text/test_text.yaml")

    # Get both global and main sections to find the shared lambda definition
    full_cpp = CORE.cpp_global_section + main_cpp

    # Then
    # Lambda is deduplicated into a shared function (reference in main section)
    assert "it_4->set_template(shared_lambda_" in main_cpp
    # Lambda body should be in the code somewhere
    assert 'return std::string{"Hello"};' in full_cpp
    # Verify the shared lambda function is defined (in global section)
    assert "esphome::optional<std::string> shared_lambda_" in full_cpp

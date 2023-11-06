"""Tests for the binary sensor component."""


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
    assert 'it_1->set_name("test 1 text");' in main_cpp


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

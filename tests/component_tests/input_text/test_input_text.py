"""Tests for the binary sensor component."""


def test_input_text_is_setup(generate_main):
    """
    When the binary sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/input_text/test_input_text.yaml"
    )

    # Then
    assert "input_text = new input_text::InputTextComponent();" in main_cpp
    assert "App.register_component(input_text);" in main_cpp


def test_input_text_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/input_text/test_input_text.yaml"
    )

    # Then
    assert 'it_1->set_name("test it1");' in main_cpp


def test_input_text_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/input_text/test_input_text.yaml"
    )

    # Then
    assert "it_1->set_internal(true);" in main_cpp
    assert "it_2->set_internal(false);" in main_cpp

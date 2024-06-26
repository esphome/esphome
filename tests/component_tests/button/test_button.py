"""Tests for the button component"""


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
    assert 'wol_1->set_name("wol_test_1");' in main_cpp
    assert "wol_2->set_macaddr(18, 52, 86, 120, 144, 171);" in main_cpp


def test_button_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/button/test_button.yaml")

    # Then
    assert "wol_1->set_internal(true);" in main_cpp
    assert "wol_2->set_internal(false);" in main_cpp

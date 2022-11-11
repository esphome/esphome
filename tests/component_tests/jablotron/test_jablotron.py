"""Tests for the jablotron component."""


def test_jablotron_esp8266(generate_main):
    """
    When the index and flag of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/jablotron/test_jablotron_esp8266.yaml"
    )

    # Then
    assert 'jablotron_jablotroncomponent->set_access_code("1234");' in main_cpp
    assert "entry_delay->set_index(1);" in main_cpp
    assert "entry_delay->set_flag(6);" in main_cpp
    assert (
        "entry_delay->set_parent_jablotron(jablotron_jablotroncomponent);" in main_cpp
    )


def test_jablotron_esp32(generate_main):
    """
    When the entity category of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/jablotron/test_jablotron_esp32.yaml"
    )

    # Then
    assert 'jablo->set_access_code("1234");' in main_cpp
    assert "jablo_info = new jablotron_info::InfoSensor();" in main_cpp
    assert "jablo_info->set_parent_jablotron(jablo);" in main_cpp
    assert 'jablo_info->set_name("Jablotron Info");' in main_cpp
    assert "jablo_info->set_entity_category(::ENTITY_CATEGORY_DIAGNOSTIC);" in main_cpp


def test_jablotron_esp32idf(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/jablotron/test_jablotron_esp32idf.yaml"
    )

    # Then
    assert 'entry_motion->set_name("Entryway motion");' in main_cpp
    assert "entry_motion->set_disabled_by_default(false);" in main_cpp
    assert 'entry_motion->set_device_class("motion");' in main_cpp
    assert "entry_motion->set_index(2);" in main_cpp
    assert "entry_motion->set_parent_jablotron(jablo);" in main_cpp

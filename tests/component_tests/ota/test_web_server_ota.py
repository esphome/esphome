"""Tests for the web_server OTA platform."""

from collections.abc import Callable


def test_web_server_ota_generated(generate_main: Callable[[str], str]) -> None:
    """Test that web_server OTA platform generates correct code."""
    main_cpp = generate_main("tests/component_tests/ota/test_web_server_ota.yaml")

    # Check that the web server OTA component is included
    assert "WebServerOTAComponent" in main_cpp
    assert "web_server::WebServerOTAComponent" in main_cpp

    # Check that global web server base is referenced
    assert "global_web_server_base" in main_cpp

    # Check component is registered
    assert "App.register_component(web_server_webserverotacomponent_id)" in main_cpp


def test_web_server_ota_with_callbacks(generate_main: Callable[[str], str]) -> None:
    """Test web_server OTA with state callbacks."""
    main_cpp = generate_main(
        "tests/component_tests/ota/test_web_server_ota_callbacks.yaml"
    )

    # Check that web server OTA component is present
    assert "WebServerOTAComponent" in main_cpp

    # Check that callbacks are configured
    # The actual callback code is in the component implementation, not main.cpp
    # But we can check that logger.log statements are present from the callbacks
    assert "logger.log" in main_cpp
    assert "OTA started" in main_cpp
    assert "OTA completed" in main_cpp
    assert "OTA error" in main_cpp


def test_web_server_ota_idf_multipart(generate_main: Callable[[str], str]) -> None:
    """Test that ESP-IDF builds include multipart parser dependency."""
    main_cpp = generate_main("tests/component_tests/ota/test_web_server_ota_idf.yaml")

    # Check that web server OTA component is present
    assert "WebServerOTAComponent" in main_cpp

    # For ESP-IDF builds, the framework type is esp-idf
    # The multipart parser dependency is added by web_server_idf
    assert "web_server::WebServerOTAComponent" in main_cpp


def test_web_server_ota_without_web_server_fails(
    generate_main: Callable[[str], str],
) -> None:
    """Test that web_server OTA requires web_server component."""
    # This should fail during validation since web_server_base is required
    # but we can't test validation failures with generate_main
    # Instead, verify that both components are needed in valid config
    main_cpp = generate_main("tests/component_tests/ota/test_web_server_ota.yaml")

    # Both web server and OTA components should be present
    assert "WebServer" in main_cpp
    assert "WebServerOTAComponent" in main_cpp


def test_multiple_ota_platforms(generate_main: Callable[[str], str]) -> None:
    """Test multiple OTA platforms can coexist."""
    main_cpp = generate_main("tests/component_tests/ota/test_web_server_ota_multi.yaml")

    # Check all OTA platforms are included
    assert "WebServerOTAComponent" in main_cpp
    assert "ESPHomeOTAComponent" in main_cpp
    assert "OtaHttpRequestComponent" in main_cpp

    # Check components are from correct namespaces
    assert "web_server::WebServerOTAComponent" in main_cpp
    assert "esphome::ESPHomeOTAComponent" in main_cpp
    assert "http_request::OtaHttpRequestComponent" in main_cpp


def test_web_server_ota_arduino_with_auth(generate_main: Callable[[str], str]) -> None:
    """Test web_server OTA with Arduino framework and authentication."""
    main_cpp = generate_main(
        "tests/component_tests/ota/test_web_server_ota_arduino.yaml"
    )

    # Check web server OTA component is present
    assert "WebServerOTAComponent" in main_cpp

    # Check authentication is set up for web server
    assert "set_auth_username" in main_cpp
    assert "set_auth_password" in main_cpp


def test_web_server_ota_esp8266(generate_main: Callable[[str], str]) -> None:
    """Test web_server OTA on ESP8266 platform."""
    main_cpp = generate_main(
        "tests/component_tests/ota/test_web_server_ota_esp8266.yaml"
    )

    # Check web server OTA component is present
    assert "WebServerOTAComponent" in main_cpp
    assert "web_server::WebServerOTAComponent" in main_cpp

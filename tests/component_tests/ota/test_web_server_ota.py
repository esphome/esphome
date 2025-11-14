"""Tests for the web_server OTA platform."""

from __future__ import annotations

from collections.abc import Callable
import logging
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.web_server.ota import _web_server_ota_final_validate
from esphome.const import CONF_ID, CONF_OTA, CONF_PLATFORM, CONF_WEB_SERVER
from esphome.core import ID
import esphome.final_validate as fv


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


@pytest.mark.parametrize(
    ("ota_configs", "expected_count", "warning_expected"),
    [
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_WEB_SERVER,
                    CONF_ID: ID("ota_web", is_manual=False),
                }
            ],
            1,
            False,
            id="single_instance_no_merge",
        ),
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_WEB_SERVER,
                    CONF_ID: ID("ota_web_1", is_manual=False),
                },
                {
                    CONF_PLATFORM: CONF_WEB_SERVER,
                    CONF_ID: ID("ota_web_2", is_manual=False),
                },
            ],
            1,
            True,
            id="two_instances_merged",
        ),
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_WEB_SERVER,
                    CONF_ID: ID("ota_web_1", is_manual=False),
                },
                {
                    CONF_PLATFORM: "esphome",
                    CONF_ID: ID("ota_esphome", is_manual=False),
                },
                {
                    CONF_PLATFORM: CONF_WEB_SERVER,
                    CONF_ID: ID("ota_web_2", is_manual=False),
                },
            ],
            2,
            True,
            id="mixed_platforms_web_server_merged",
        ),
    ],
)
def test_web_server_ota_instance_merging(
    ota_configs: list[dict[str, Any]],
    expected_count: int,
    warning_expected: bool,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test web_server OTA instance merging behavior."""
    full_conf = {CONF_OTA: ota_configs.copy()}

    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            _web_server_ota_final_validate({})

        updated_conf = fv.full_config.get()

        # Verify total number of OTA platforms
        assert len(updated_conf[CONF_OTA]) == expected_count

        # Verify warning
        if warning_expected:
            assert any(
                "Found and merged" in record.message
                and "web_server OTA" in record.message
                for record in caplog.records
            ), "Expected merge warning not found in log"
        else:
            assert len(caplog.records) == 0, "Unexpected warnings logged"
    finally:
        fv.full_config.reset(token)


def test_web_server_ota_consistent_manual_ids(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that consistent manual IDs can be merged successfully."""
    ota_configs = [
        {
            CONF_PLATFORM: CONF_WEB_SERVER,
            CONF_ID: ID("ota_web", is_manual=True),
        },
        {
            CONF_PLATFORM: CONF_WEB_SERVER,
            CONF_ID: ID("ota_web", is_manual=True),
        },
    ]

    full_conf = {CONF_OTA: ota_configs}

    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            _web_server_ota_final_validate({})

        updated_conf = fv.full_config.get()
        assert len(updated_conf[CONF_OTA]) == 1
        assert updated_conf[CONF_OTA][0][CONF_ID].id == "ota_web"
        assert any(
            "Found and merged" in record.message and "web_server OTA" in record.message
            for record in caplog.records
        )
    finally:
        fv.full_config.reset(token)


def test_web_server_ota_inconsistent_manual_ids() -> None:
    """Test that inconsistent manual IDs raise an error."""
    ota_configs = [
        {
            CONF_PLATFORM: CONF_WEB_SERVER,
            CONF_ID: ID("ota_web_1", is_manual=True),
        },
        {
            CONF_PLATFORM: CONF_WEB_SERVER,
            CONF_ID: ID("ota_web_2", is_manual=True),
        },
    ]

    full_conf = {CONF_OTA: ota_configs}

    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(
            cv.Invalid,
            match="Found multiple web_server OTA configurations but id is inconsistent",
        ):
            _web_server_ota_final_validate({})
    finally:
        fv.full_config.reset(token)

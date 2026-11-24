"""Tests for the http_request update platform's configuration validation."""

import logging

import pytest

from esphome.components.const import CONF_VERIFY_SSL
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, DOMAIN
from esphome.components.http_request.update import (
    CONF_ALLOW_AUTO_INSTALL,
    _final_validate,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_ID,
    CONF_NAME,
    CONF_PROJECT,
    CONF_SOURCE,
    CONF_VERSION,
)
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType

PROJECT: ConfigType = {CONF_PROJECT: {CONF_NAME: "esphome.test", CONF_VERSION: "1.1"}}


def _full_config(esphome_config: ConfigType, verify_ssl: bool = True) -> Config:
    request_id = ID("request", is_declaration=True)
    config = Config()
    config[CONF_ESPHOME] = esphome_config
    config[DOMAIN] = {CONF_ID: request_id, CONF_VERIFY_SSL: verify_ssl}
    config.declare_ids.append((request_id, [DOMAIN, CONF_ID]))
    return config


def _platform_config(
    *,
    allow_auto_install: bool = True,
    source: str = "https://example.com/manifest.json",
) -> ConfigType:
    return {
        CONF_ALLOW_AUTO_INSTALL: allow_auto_install,
        CONF_SOURCE: source,
        CONF_HTTP_REQUEST_ID: ID("request"),
    }


def _run_final_validate(
    config: ConfigType, esphome_config: ConfigType, verify_ssl: bool = True
) -> None:
    token = fv.full_config.set(_full_config(esphome_config, verify_ssl))
    try:
        _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_auto_install_requires_a_project_version() -> None:
    with pytest.raises(cv.Invalid, match="'project'"):
        _run_final_validate(_platform_config(), {})


def test_auto_install_accepts_a_verified_https_manifest(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        _run_final_validate(_platform_config(), PROJECT)
    assert "network path" not in caplog.text


def test_auto_install_warns_about_a_plain_http_manifest(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        _run_final_validate(
            _platform_config(source="http://example.com/manifest.json"), PROJECT
        )
    assert "is not fetched over HTTPS" in caplog.text


def test_auto_install_warns_when_certificates_are_not_verified(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        _run_final_validate(_platform_config(), PROJECT, verify_ssl=False)
    assert "does not verify TLS certificates" in caplog.text


def test_auto_install_disabled_needs_no_project_or_secure_transport(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        _run_final_validate(
            _platform_config(
                allow_auto_install=False, source="http://example.com/manifest.json"
            ),
            {},
            verify_ssl=False,
        )
    assert "network path" not in caplog.text

"""Tests for the esphome OTA platform final_validate logic."""

from __future__ import annotations

import logging
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.esphome.ota import ota_esphome_final_validate
from esphome.const import (
    CONF_ESPHOME,
    CONF_ID,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_VERSION,
)
from esphome.core import ID
import esphome.final_validate as fv


def _make_ota_config(port: int = 3232, **kwargs: Any) -> dict[str, Any]:
    config: dict[str, Any] = {
        CONF_PLATFORM: CONF_ESPHOME,
        CONF_ID: ID(f"ota_esphome_{port}", is_manual=False),
        CONF_VERSION: 2,
        CONF_PORT: port,
    }
    config.update(kwargs)
    return config


def test_single_esphome_ota_instance_accepted() -> None:
    """A single ESPHome OTA config passes final_validate untouched."""
    full_conf = {CONF_OTA: [_make_ota_config(port=3232)]}
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 1
        assert updated[CONF_OTA][0][CONF_PORT] == 3232
    finally:
        fv.full_config.reset(token)


def test_same_port_configs_merge(caplog: pytest.LogCaptureFixture) -> None:
    """Two ESPHome OTA configs on the same port merge into one instance."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_PASSWORD: "pw"}),
            _make_ota_config(port=3232),
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 1
        assert updated[CONF_OTA][0][CONF_PORT] == 3232
        assert any("Found and merged" in record.message for record in caplog.records), (
            "Expected merge warning not found in log"
        )
    finally:
        fv.full_config.reset(token)


def test_multiple_ports_rejected() -> None:
    """Two ESPHome OTA configs on different ports raise cv.Invalid."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232),
            _make_ota_config(port=3233),
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(
            cv.Invalid,
            match=r"Only a single port is supported for 'ota' 'platform: esphome'",
        ):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_non_esphome_ota_unaffected() -> None:
    """Non-esphome OTA platforms are not subject to the single-instance rule."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232),
            {CONF_PLATFORM: "web_server", CONF_ID: ID("ota_ws", is_manual=False)},
            {CONF_PLATFORM: "http_request", CONF_ID: ID("ota_hr", is_manual=False)},
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 3
    finally:
        fv.full_config.reset(token)

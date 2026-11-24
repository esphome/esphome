"""Tests for the http_request update platform's configuration validation."""

import pytest

from esphome.components.http_request.update import (
    CONF_ALLOW_AUTO_INSTALL,
    _final_validate,
)
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, CONF_NAME, CONF_PROJECT, CONF_VERSION
import esphome.final_validate as fv
from esphome.types import ConfigType


def _run_final_validate(config: ConfigType, esphome_config: ConfigType) -> None:
    token = fv.full_config.set({CONF_ESPHOME: esphome_config})
    try:
        _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_auto_install_requires_a_project_version() -> None:
    with pytest.raises(cv.Invalid, match="'project'"):
        _run_final_validate({CONF_ALLOW_AUTO_INSTALL: True}, {})


def test_auto_install_accepts_a_project_version() -> None:
    _run_final_validate(
        {CONF_ALLOW_AUTO_INSTALL: True},
        {CONF_PROJECT: {CONF_NAME: "esphome.test", CONF_VERSION: "1.1"}},
    )


def test_auto_install_disabled_needs_no_project_version() -> None:
    _run_final_validate({CONF_ALLOW_AUTO_INSTALL: False}, {})

"""Validate microphone HTTP stream PSRAM requirements."""

from collections.abc import Generator

import pytest

from esphome import config_validation as cv
from esphome.components.microphone_http_stream import (
    CONF_MAX_CLIENTS,
    FINAL_VALIDATE_SCHEMA,
)
from esphome.config import Config
from esphome.const import CONF_DISABLED
import esphome.final_validate as fv


@pytest.fixture
def full_config() -> Generator[Config, None, None]:
    config = Config()
    token = fv.full_config.set(config)
    yield config
    fv.full_config.reset(token)


def test_single_client_does_not_require_psram(full_config: Config) -> None:
    config = {CONF_MAX_CLIENTS: 1}
    assert FINAL_VALIDATE_SCHEMA(config)[CONF_MAX_CLIENTS] == 1


@pytest.mark.parametrize("max_clients", [2, 3])
def test_multiple_clients_require_psram(full_config: Config, max_clients: int) -> None:
    with pytest.raises(cv.Invalid, match="requires enabled psram"):
        FINAL_VALIDATE_SCHEMA({CONF_MAX_CLIENTS: max_clients})


@pytest.mark.parametrize("max_clients", [2, 3])
def test_multiple_clients_reject_disabled_psram(
    full_config: Config, max_clients: int
) -> None:
    full_config["psram"] = {CONF_DISABLED: True}
    with pytest.raises(cv.Invalid, match="requires enabled psram"):
        FINAL_VALIDATE_SCHEMA({CONF_MAX_CLIENTS: max_clients})


@pytest.mark.parametrize("max_clients", [2, 3])
def test_multiple_clients_accept_enabled_psram(
    full_config: Config, max_clients: int
) -> None:
    full_config["psram"] = {CONF_DISABLED: False}
    config = {CONF_MAX_CLIENTS: max_clients}
    assert FINAL_VALIDATE_SCHEMA(config)[CONF_MAX_CLIENTS] == max_clients

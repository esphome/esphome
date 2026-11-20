"""Validate microphone HTTP stream PSRAM requirements."""

from collections.abc import Callable
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_IGNORE_NOT_FOUND
from esphome.components.microphone_http_stream import (
    CONF_MAX_CLIENTS,
    FINAL_VALIDATE_SCHEMA,
)
from esphome.const import CONF_DISABLED


def test_single_client_does_not_require_psram() -> None:
    config = {CONF_MAX_CLIENTS: 1}
    assert FINAL_VALIDATE_SCHEMA(config)[CONF_MAX_CLIENTS] == 1


@pytest.mark.parametrize(
    "psram_config",
    [None, {CONF_DISABLED: True, CONF_IGNORE_NOT_FOUND: False}],
    ids=["missing", "disabled"],
)
@pytest.mark.parametrize("max_clients", [2, 3])
def test_multiple_clients_require_enabled_psram(
    set_component_config: Callable[[str, Any], None],
    max_clients: int,
    psram_config: dict | None,
) -> None:
    if psram_config is not None:
        set_component_config("psram", psram_config)
    with pytest.raises(cv.Invalid, match="requires enabled psram"):
        FINAL_VALIDATE_SCHEMA({CONF_MAX_CLIENTS: max_clients})


@pytest.mark.parametrize("ignore_not_found", [True, False])
@pytest.mark.parametrize("max_clients", [2, 3])
def test_multiple_clients_accept_enabled_psram(
    set_component_config: Callable[[str, Any], None],
    max_clients: int,
    ignore_not_found: bool,
) -> None:
    set_component_config(
        "psram", {CONF_DISABLED: False, CONF_IGNORE_NOT_FOUND: ignore_not_found}
    )
    config = {CONF_MAX_CLIENTS: max_clients}
    assert FINAL_VALIDATE_SCHEMA(config)[CONF_MAX_CLIENTS] == max_clients

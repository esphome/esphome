"""transmit_pronto only sends data; delta is a receive-side match tolerance."""

import pytest

from esphome import config_validation as cv
from esphome.automation import ACTION_REGISTRY
from esphome.components.remote_base import PRONTO_BINARY_SENSOR_SCHEMA
import esphome.components.remote_transmitter  # noqa: F401

PRONTO_DATA = "0000 006D 0001 0000 0015 0016"


def test_transmit_rejects_delta() -> None:
    with pytest.raises(cv.Invalid, match="delta"):
        ACTION_REGISTRY["remote_transmitter.transmit_pronto"].schema(
            {"data": PRONTO_DATA, "delta": 10}
        )


def test_transmit_accepts_data() -> None:
    config = ACTION_REGISTRY["remote_transmitter.transmit_pronto"].schema(
        {"data": PRONTO_DATA}
    )
    assert config["data"] == PRONTO_DATA


def test_binary_sensor_schema_keeps_delta() -> None:
    config = PRONTO_BINARY_SENSOR_SCHEMA({"data": PRONTO_DATA, "delta": 10})
    assert config["delta"] == 10

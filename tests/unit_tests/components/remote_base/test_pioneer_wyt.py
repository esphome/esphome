"""Schema checks for the pioneer_wyt transmit action."""

import pytest

from esphome.automation import ACTION_REGISTRY
import esphome.config_validation as cv
from esphome.core import Lambda

# Factory remote capture: cool mode, 26°C, without the trailing checksum byte
RAW_CODE = [
    0x23,
    0xCB,
    0x26,
    0x01,
    0x00,
    0x24,
    0x03,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
]
RAW_CODE_CHECKSUM = 0xC1


@pytest.fixture
def schema() -> cv.Schema:
    return ACTION_REGISTRY["remote_transmitter.transmit_pioneer_wyt"].raw_schema


def test_code_checksum_matches_capture() -> None:
    assert sum(RAW_CODE) & 0xFF == RAW_CODE_CHECKSUM


@pytest.mark.parametrize("field", [{"power": True}, {"type": "BOTH"}])
def test_code_rejects_friendly_fields(schema: cv.Schema, field: dict) -> None:
    assert schema({"code": RAW_CODE})["code"] == RAW_CODE
    with pytest.raises(cv.Invalid, match="'code' cannot be combined with"):
        schema({"code": RAW_CODE, **field})


def test_code_rejects_friendly_lambda(schema: cv.Schema) -> None:
    with pytest.raises(cv.Invalid, match="'code' cannot be combined with mode"):
        schema({"code": RAW_CODE, "mode": Lambda("return PIONEER_WYT_MODE_COOL;")})


def test_code_accepts_with_and_without_checksum_byte(schema: cv.Schema) -> None:
    with_checksum = [*RAW_CODE, RAW_CODE_CHECKSUM]
    assert schema({"code": RAW_CODE})["code"] == RAW_CODE
    assert schema({"code": with_checksum})["code"] == with_checksum


@pytest.mark.parametrize("code", [RAW_CODE[:-1], [*RAW_CODE, RAW_CODE_CHECKSUM, 0x00]])
def test_code_rejects_other_lengths(schema: cv.Schema, code: list[int]) -> None:
    with pytest.raises(cv.Invalid):
        schema({"code": code})


@pytest.mark.parametrize("value", [16.0, 22.5, 31.0])
def test_target_temperature_accepts_half_degree_steps(
    schema: cv.Schema, value: float
) -> None:
    assert schema({"target_temperature": value})["target_temperature"] == value


@pytest.mark.parametrize("value", [15.5, 22.3, 31.5])
def test_target_temperature_rejects_bad_values(schema: cv.Schema, value: float) -> None:
    with pytest.raises(cv.Invalid):
        schema({"target_temperature": value})

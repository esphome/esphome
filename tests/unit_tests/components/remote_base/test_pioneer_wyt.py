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


def test_code_accepts_with_and_without_checksum_byte(schema: cv.Schema) -> None:
    with_checksum = [*RAW_CODE, RAW_CODE_CHECKSUM]
    assert schema({"code": RAW_CODE})["code"] == RAW_CODE
    assert schema({"code": with_checksum})["code"] == with_checksum


@pytest.mark.parametrize("code", [RAW_CODE[:-1], [*RAW_CODE, RAW_CODE_CHECKSUM, 0x00]])
def test_code_rejects_other_lengths(schema: cv.Schema, code: list[int]) -> None:
    with pytest.raises(cv.Invalid):
        schema({"code": code})


def test_code_accepts_lambda(schema: cv.Schema) -> None:
    code = Lambda("return {0x23, 0xCB};")
    assert isinstance(schema({"code": code})["code"], Lambda)


def test_code_is_required(schema: cv.Schema) -> None:
    with pytest.raises(cv.Invalid):
        schema({})

"""The action schema is rebuilt by templatize() so every field accepts a lambda; the
code-versus-friendly-fields check must survive that rebuild or it never runs."""

import pytest

from esphome.components.remote_base import PIONEER_WYT_ACTION_SCHEMA, templatize
import esphome.config_validation as cv

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


def test_templatize_keeps_code_exclusivity_check() -> None:
    schema = templatize(PIONEER_WYT_ACTION_SCHEMA)
    assert schema({"code": RAW_CODE}) == {"code": RAW_CODE}
    with pytest.raises(
        cv.Invalid, match="Cannot specify both code and friendly fields"
    ):
        schema({"code": RAW_CODE, "power": True})


def test_code_accepts_with_and_without_checksum_byte() -> None:
    schema = templatize(PIONEER_WYT_ACTION_SCHEMA)
    with_checksum = [*RAW_CODE, RAW_CODE_CHECKSUM]
    assert schema({"code": RAW_CODE}) == {"code": RAW_CODE}
    assert schema({"code": with_checksum}) == {"code": with_checksum}


@pytest.mark.parametrize("code", [RAW_CODE[:-1], [*RAW_CODE, RAW_CODE_CHECKSUM, 0x00]])
def test_code_rejects_other_lengths(code: list[int]) -> None:
    schema = templatize(PIONEER_WYT_ACTION_SCHEMA)
    with pytest.raises(cv.Invalid):
        schema({"code": code})


@pytest.mark.parametrize("value", [16.0, 22.5, 31.0])
def test_target_temperature_accepts_half_degree_steps(value: float) -> None:
    schema = templatize(PIONEER_WYT_ACTION_SCHEMA)
    assert schema({"target_temperature": value}) == {"target_temperature": value}


@pytest.mark.parametrize("value", [15.5, 22.3, 31.5])
def test_target_temperature_rejects_bad_values(value: float) -> None:
    schema = templatize(PIONEER_WYT_ACTION_SCHEMA)
    with pytest.raises(cv.Invalid):
        schema({"target_temperature": value})

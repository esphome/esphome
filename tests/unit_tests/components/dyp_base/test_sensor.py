from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_OUTPUT_MODE
from esphome.components.dyp_a13 import sensor as dyp_a13_sensor
from esphome.components.dyp_base import (
    CONF_BLIND_ZONE,
    CONF_OUTPUT_MODE_PIN,
    CONF_PUBLISH_MODE,
    MODES,
    OUTPUT_MODES,
    PUBLISH_MODES,
    DYPModel,
    dyp_sensor_schema,
    to_code_base,
)
from esphome.const import (
    CONF_ID,
    CONF_MAX_RANGE,
    CONF_MODE,
    CONF_MODEL,
    CONF_TRIGGER_PIN,
)

NO_OUTPUT_MODE_SCHEMA = dyp_sensor_schema(
    {"NO_OUT": DYPModel("25cm", "800cm", mode="AUTO", supports_rx_mode_select=False)}
)


def test_accepts_lowercase_model_and_applies_defaults(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(cv, "use_id", lambda *_args, **_kwargs: cv.string)
    schema = dyp_a13_sensor.CONFIG_SCHEMA
    validated = schema({CONF_MODEL: "a13nytw", "name": "Sump Pit"})
    assert validated[CONF_MODEL] == "A13NYTW"
    assert validated[CONF_MODE] == "CONTROLLED"
    assert validated[CONF_BLIND_ZONE] == pytest.approx(0.25)
    assert validated[CONF_MAX_RANGE] == pytest.approx(2.0)


def test_allows_overriding_blind_zone_and_max_range(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(cv, "use_id", lambda *_args, **_kwargs: cv.string)
    schema = dyp_a13_sensor.CONFIG_SCHEMA
    validated = schema(
        {
            CONF_MODEL: "A13NYTW",
            "name": "Sump Pit",
            CONF_BLIND_ZONE: "10cm",
            CONF_MAX_RANGE: "300cm",
        }
    )
    assert validated[CONF_BLIND_ZONE] == pytest.approx(0.10)
    assert validated[CONF_MAX_RANGE] == pytest.approx(3.0)


def test_unknown_model_raises_cv_invalid() -> None:
    with pytest.raises(cv.Invalid):
        dyp_a13_sensor.CONFIG_SCHEMA({CONF_MODEL: "NOT_A_MODEL", "name": "Bad"})


@pytest.mark.parametrize(
    ("schema", "raw_config", "expected_msg"),
    [
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {CONF_MODEL: "A13NYUW", "name": "Test", CONF_MODE: "AUTO"},
            "extra keys not allowed",
        ),
        (
            NO_OUTPUT_MODE_SCHEMA,
            {CONF_MODEL: "NO_OUT", "name": "Test", CONF_OUTPUT_MODE: "PROCESSED"},
            "extra keys not allowed",
        ),
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {
                CONF_MODEL: "A13",
                "name": "Test",
                CONF_MODE: "AUTO",
                CONF_TRIGGER_PIN: 5,
            },
            "`trigger_pin` is not allowed when `mode` is 'AUTO'",
        ),
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {
                CONF_MODEL: "A13NYUW",
                "name": "Test",
                CONF_OUTPUT_MODE: "REALTIME",
            },
            "'output_mode' requires 'output_mode_pin' to be configured",
        ),
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {
                CONF_MODEL: "A13",
                "name": "Test",
                CONF_MODE: "CONTROLLED",
                CONF_OUTPUT_MODE_PIN: 4,
            },
            "'output_mode' and 'output_mode_pin' are only allowed when 'mode' is 'AUTO'",
        ),
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {
                CONF_MODEL: "A13NYTW",
                "name": "Test",
                CONF_BLIND_ZONE: "-0.1m",
            },
            "value must be at least 0.0",
        ),
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {
                CONF_MODEL: "A13NYTW",
                "name": "Test",
                CONF_MAX_RANGE: "100m",
            },
            "value must be at most 65.535",
        ),
        (
            dyp_a13_sensor.CONFIG_SCHEMA,
            {
                CONF_MODEL: "A13NYTW",
                "name": "Test",
                CONF_BLIND_ZONE: "3m",
                CONF_MAX_RANGE: "2m",
            },
            "'blind_zone' must be less than 'max_range'",
        ),
    ],
)
def test_invalid_user_configs(
    monkeypatch: pytest.MonkeyPatch,
    schema: Any,
    raw_config: dict[str, Any],
    expected_msg: str,
) -> None:
    from esphome import pins

    monkeypatch.setattr(cv, "use_id", lambda *_args, **_kwargs: cv.string)
    monkeypatch.setattr(pins.PIN_SCHEMA_REGISTRY, "validate", lambda v, *_a, **_kw: v)
    with pytest.raises(cv.Invalid, match=expected_msg):
        schema(raw_config)


@pytest.mark.asyncio
async def test_codegen_auto_mode_with_output_mode_pin() -> None:
    mock_var = MagicMock()
    mock_pin = MagicMock()
    config: dict[str, Any] = {
        CONF_ID: "test_id",
        CONF_MODE: MODES["AUTO"],
        CONF_BLIND_ZONE: 0.25,
        CONF_MAX_RANGE: 2.0,
        CONF_PUBLISH_MODE: PUBLISH_MODES["IMMEDIATE"],
        CONF_OUTPUT_MODE: OUTPUT_MODES["REALTIME"],
        CONF_OUTPUT_MODE_PIN: {"number": 4},
    }
    with (
        patch(
            "esphome.components.dyp_base.sensor.new_sensor",
            new=AsyncMock(return_value=mock_var),
        ),
        patch(
            "esphome.components.dyp_base.cg.register_component",
            new=AsyncMock(),
        ),
        patch(
            "esphome.components.dyp_base.uart.register_uart_device",
            new=AsyncMock(),
        ),
        patch(
            "esphome.components.dyp_base.cg.add_pin",
            new=AsyncMock(return_value=mock_pin),
        ) as mock_add_pin,
        patch("esphome.components.dyp_base.cg.add") as mock_cg_add,
        patch("esphome.components.dyp_base.cg.add_optional") as mock_cg_add_opt,
    ):
        await to_code_base(config)

    assert mock_cg_add.call_count == 3
    assert mock_cg_add_opt.call_count == 2
    assert mock_add_pin.await_count == 2

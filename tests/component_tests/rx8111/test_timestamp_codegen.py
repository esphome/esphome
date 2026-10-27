"""Check the timestamp platform's parent, entity, and mode wiring."""

from unittest import mock

import pytest

from esphome.components.rx8111 import text_sensor as rx8111


@pytest.mark.asyncio
@pytest.mark.parametrize("mode", ["latest", "stop_when_full", "overwrite"])
async def test_timestamp_codegen(mode: str) -> None:
    config = rx8111.CONFIG_SCHEMA(
        {"name": "Timestamp", "rx8111_id": "rtc", "timestamp_record_mode": mode}
    )
    parent = mock.MagicMock()
    sensor = mock.MagicMock()
    with (
        mock.patch.object(
            rx8111.cg, "get_variable", mock.AsyncMock(return_value=parent)
        ),
        mock.patch.object(
            rx8111.text_sensor, "new_text_sensor", mock.AsyncMock(return_value=sensor)
        ),
        mock.patch.object(rx8111.cg, "add"),
    ):
        await rx8111.to_code(config)
    parent.set_event_timestamp_sensor.assert_called_once_with(
        sensor, config[rx8111.CONF_TIMESTAMP_RECORD_MODE]
    )
    assert (
        config[rx8111.CONF_TIMESTAMP_RECORD_MODE].enum_value
        is rx8111.TIMESTAMP_MODES[mode]
    )

"""Select commands use verified P2 writes and retain other confirmed API states during context refresh."""

import asyncio
import re

from aioesphomeapi import ButtonInfo, NumberInfo, NumberState, SelectInfo, SelectState
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
@pytest.mark.shared_yaml("uart_mock_vbus_setting_write")
async def test_uart_mock_vbus_selects(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    initial = asyncio.Event()
    verified = asyncio.Event()
    unchanged = asyncio.Event()
    states: dict[int, NumberState | SelectState] = {}
    languages: list[str] = []
    numbers: list[float] = []
    missing_after_confirmation: list[bool] = []
    states_at_write: list[tuple[str, float]] = []
    language_key = 0
    minimum_key = 0

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.wire_test:" in line:
            frame = bytes.fromhex(text.removesuffix(" (16)").replace(".", " "))
            if frame[7] == 2:
                states_at_write.append(
                    (states[language_key].state, states[minimum_key].state)
                )
        if "[vbus.write_test:" in line and text.split()[1] == "4":
            unchanged.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        language = find_entity(entities, "display_language", SelectInfo)
        minimum = find_entity(entities, "minimum_pump_speed", NumberInfo)
        assert start is not None and language is not None and minimum is not None
        language_key, minimum_key = language.key, minimum.key

        def on_state(state: NumberState | SelectState) -> None:
            if not isinstance(state, (NumberState, SelectState)):
                return
            if state.key not in (language_key, minimum_key):
                return
            states[state.key] = state
            values = languages if state.key == language_key else numbers
            if values:
                missing_after_confirmation.append(state.missing_state)
            if not state.missing_state:
                values.append(state.state)
            if languages and numbers:
                initial.set()
            if languages and languages[-1] == "French":
                verified.set()

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(initial.wait(), timeout=12)
        assert languages == ["Italian"] and numbers == [35]
        client.select_command(language.key, "French")
        assert states[language.key].state == "Italian"
        await asyncio.wait_for(verified.wait(), timeout=12)
        assert languages == ["Italian", "French"]
        assert numbers == [35]
        assert states_at_write == [("Italian", 35)]
        client.select_command(language.key, "French")
        await asyncio.wait_for(unchanged.wait(), timeout=5)
        await asyncio.sleep(0.1)
        assert languages == ["Italian", "French"]
        assert numbers == [35]
        assert states_at_write == [("Italian", 35)]
        assert not any(missing_after_confirmation)

        # Metadata listing explicitly resends states, so check it after the duplicate-state assertions.
        entities, _ = await client.list_entities_services()
        flow = find_entity(entities, "flow_sensor_type", SelectInfo)
        assert flow is not None
        assert flow.options == ["Off", "12", "40"]

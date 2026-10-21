"""Integration test for LD2420 reports after the last config exit command."""

from __future__ import annotations

import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

EXIT_SENT = re.compile(r"Config exit sent (\d+)")


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("mode", "expected_sends"),
    [
        pytest.param(0, 3, id="silent"),
        pytest.param(1, 3, id="nack"),
        pytest.param(2, 3, id="short"),
        pytest.param(3, 1, id="ack"),
    ],
)
async def test_uart_mock_ld2420_config_exit(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    mode: int,
    expected_sends: int,
) -> None:
    """Test both energy reports arrive after the last config exit, and a failed exit is still sent 3 times."""
    exit_sends: list[int] = []

    def line_callback(line: str) -> None:
        if match := EXIT_SENT.search(line):
            exit_sends.append(int(match[1]))

    collector = SensorStateCollector(sensor_names=["moving_distance"])
    yaml_config = yaml_config.replace("EXIT_REPLY_MODE", str(mode))

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        resumed = collector.add_waiter(
            lambda: len(collector.sensor_states["moving_distance"]) >= 2
        )
        await wait_or_fail(
            resumed,
            5.0,
            "Energy reports did not resume after the last config exit",
            exit_sends=exit_sends,
            sensor_states=collector.sensor_states,
        )

        # The sensor skips repeated values, so each distinct distance is one report
        assert collector.sensor_states["moving_distance"] == pytest.approx(
            [100.0, 101.0]
        )
        # A failed exit is resent and then given up; it is never treated as answered
        assert exit_sends == list(range(1, expected_sends + 1))

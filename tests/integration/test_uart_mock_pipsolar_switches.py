"""Integration test for the pipsolar switches with mock UART.

Each write queues the switch's on or off command on the hub, a direction with
no command queues nothing, nothing is written at boot, and the write itself
does not publish the switch state (the hub's polled status does).
"""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

from aioesphomeapi import EntityState, SwitchInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

QUEUED = re.compile(r"Command queued successfully: (\w+) at position")


@pytest.mark.asyncio
async def test_uart_mock_pipsolar_switches(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Switch writes queue their commands and publish nothing themselves."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        utility = require_entity(entities, "priority_utility", SwitchInfo)
        voltage_range = require_entity(entities, "input_voltage_range", SwitchInfo)
        balance = require_entity(entities, "pv_power_balance", SwitchInfo)

        states: list[EntityState] = []
        initial = asyncio.get_running_loop().create_future()

        def on_state(state: EntityState) -> None:
            states.append(state)
            if len(states) == 3 and not initial.done():
                initial.set_result(None)

        client.subscribe_states(on_state)
        await asyncio.wait_for(initial, timeout=5.0)
        assert all(state.missing_state for state in states)
        # Nothing is restored or written at boot
        assert waiter.count(QUEUED) == 0

        async def queued_after(key: int, on: bool) -> str:
            future = waiter.expect(QUEUED)
            client.switch_command(key, on)
            match = await waiter.wait(future)
            return match.group(1)

        sent = waiter.expect("Sending command from queue: POP00")
        assert await queued_after(utility.key, True) == "POP00"
        await waiter.wait(sent)

        # The utility switch has no off command, so the next queued command
        # is the one written after it
        future = waiter.expect(QUEUED)
        client.switch_command(utility.key, False)
        client.switch_command(balance.key, True)
        assert (await waiter.wait(future)).group(1) == "PSPB1"
        assert await queued_after(balance.key, False) == "PSPB0"
        assert await queued_after(voltage_range.key, True) == "PGR01"
        assert await queued_after(voltage_range.key, False) == "PGR00"

        # A fresh connection's snapshot shows the writes left every switch unpublished
        async with api_client_connected() as fresh:
            snapshot: list[EntityState] = []
            done = asyncio.get_running_loop().create_future()

            def on_snapshot(state: EntityState) -> None:
                snapshot.append(state)
                if len(snapshot) == 3 and not done.done():
                    done.set_result(None)

            fresh.subscribe_states(on_snapshot)
            await asyncio.wait_for(done, timeout=5.0)
        assert all(state.missing_state for state in snapshot), snapshot

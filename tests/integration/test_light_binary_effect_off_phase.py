"""Integration test verifying the off phase of an effect reaches an ON/OFF-only light.

Regression test for https://github.com/esphome/esphome/issues/17873. A strobe effect
encodes its dark phase as `brightness = 0` while keeping `state = true`, so that the
effect keeps running instead of being stopped by an explicit turn-off. On a dimmable
light that works, because the output is driven by `state * brightness`. On a binary
light `LightColorValues::as_binary()` looked only at `state`, so the dark phase was
silently dropped and the output stayed on forever.

Effect ticks are published with `publish: false` (so Home Assistant isn't spammed with
every frame), so the effect's actual output can't be observed via API state broadcasts.
Instead, this test reads the output component's log lines, which are written on every
update regardless of the publish flag.
"""

from __future__ import annotations

import asyncio
import re
from typing import Any

from aioesphomeapi import EntityState, LightState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_binary_effect_off_phase(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A strobe effect must drive a binary light's output both on and off."""
    output_pattern = re.compile(r"BINARY_OUTPUT:(YES|NO)")
    observed: list[bool] = []

    def on_log_line(line: str) -> None:
        if match := output_pattern.search(line):
            observed.append(match.group(1) == "YES")

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        light = next(e for e in entities if e.object_id == "test_binary_light")

        state_futures: dict[int, asyncio.Future[LightState]] = {}

        def on_state(state: EntityState) -> None:
            if isinstance(state, LightState) and state.key in state_futures:
                future = state_futures[state.key]
                if not future.done():
                    future.set_result(state)

        # ESPHome sends the current state of every entity right after connecting; drain
        # that initial burst so it can't be mistaken for the response to a command below.
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        async def send_and_wait(timeout: float = 5.0, **kwargs: Any) -> LightState:
            """Send a light command and wait for the matching state response."""
            state_futures[light.key] = asyncio.get_running_loop().create_future()
            client.light_command(key=light.key, **kwargs)
            return await asyncio.wait_for(state_futures[light.key], timeout=timeout)

        # A plain turn-on must drive the output on -- brightness defaults to 100% and
        # must not be mistaken for a dark phase.
        observed.clear()
        state = await send_and_wait(state=True)
        assert state.state is True
        assert observed and observed[-1] is True, (
            f"Plain turn-on did not switch the output on -- got {observed}"
        )

        # Run the strobe effect; both phases must reach the output.
        observed.clear()
        state = await send_and_wait(effect="Fast Strobe")
        assert state.effect == "Fast Strobe"
        # Let several effect cycles run (each phase is 50ms in the fixture).
        await asyncio.sleep(1.0)

        assert True in observed, (
            f"Strobe effect never switched the output on -- got {observed}"
        )
        assert False in observed, (
            f"Strobe effect never switched the output off; its dark phase was lost -- "
            f"got {observed}"
        )

        # Stopping the effect must leave the light usable: the brightness the effect
        # left behind must not keep the output stuck off.
        observed.clear()
        state = await send_and_wait(effect="None")
        assert state.effect == "None"
        await asyncio.sleep(0.2)
        state = await send_and_wait(state=True)
        assert state.state is True
        assert observed and observed[-1] is True, (
            f"Light stayed off after the effect stopped -- got {observed}"
        )

        # An explicit turn-off still switches the output off.
        observed.clear()
        state = await send_and_wait(state=False)
        assert state.state is False
        assert observed and observed[-1] is False, (
            f"Turn-off did not switch the output off -- got {observed}"
        )

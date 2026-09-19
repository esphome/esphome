"""Integration test verifying the off phase of an effect reaches an ON/OFF-only light.

Regression test for https://github.com/esphome/esphome/issues/17873. A strobe effect
encodes its dark phase as `brightness = 0` while keeping `state = true`, so that the
effect keeps running instead of being stopped by an explicit turn-off. On a dimmable
light that works, because the output is driven by `state * brightness`. On a binary
light the dark phase used to be dropped, so the output stayed on forever.

Effect ticks are published with `publish: false` (so Home Assistant isn't spammed with
every frame), so the effect's actual output can't be observed via API state broadcasts.
Instead, this test reads the output component's log lines, which are written on every
update regardless of the publish flag.

The output log line is emitted strictly after the API state response: `perform()`
publishes inline, but the write is deferred to the next `LightState::loop()` iteration
and then has to cross the subprocess stdout pipe. So a future is armed *before* each
command and awaited afterwards, rather than reading the last observed value.
"""

from __future__ import annotations

import asyncio
import re
from typing import Any

from aioesphomeapi import EntityState, LightState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction

OUTPUT_PATTERN = re.compile(r"BINARY_OUTPUT:(YES|NO)")


@pytest.mark.asyncio
async def test_light_binary_effect_off_phase(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A strobe effect must drive a binary light's output both on and off."""
    loop = asyncio.get_running_loop()
    observed: list[bool] = []
    pending: list[asyncio.Future[bool]] = []

    def on_log_line(line: str) -> None:
        if match := OUTPUT_PATTERN.search(line):
            value = match.group(1) == "YES"
            observed.append(value)
            while pending:
                future = pending.pop(0)
                if not future.done():
                    future.set_result(value)
                    break

    def arm_output() -> asyncio.Future[bool]:
        """Arm a future for the next output write, before sending the command."""
        future: asyncio.Future[bool] = loop.create_future()
        pending.append(future)
        return future

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
            state_futures[light.key] = loop.create_future()
            client.light_command(key=light.key, **kwargs)
            return await asyncio.wait_for(state_futures[light.key], timeout=timeout)

        # A plain turn-on must drive the output on -- brightness defaults to 100% and
        # must not be mistaken for a dark phase.
        output = arm_output()
        state = await send_and_wait(state=True)
        assert state.state is True
        assert await asyncio.wait_for(output, timeout=5.0) is True, (
            "Plain turn-on did not switch the output on"
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

        # Stopping the effect must leave the light usable.
        state = await send_and_wait(effect="None")
        assert state.effect == "None"
        output = arm_output()
        state = await send_and_wait(state=True)
        assert state.state is True
        assert await asyncio.wait_for(output, timeout=5.0) is True, (
            "Light stayed off after the effect stopped"
        )

        # An explicit turn-off still switches the output off.
        output = arm_output()
        state = await send_and_wait(state=False)
        assert state.state is False
        assert await asyncio.wait_for(output, timeout=5.0) is False, (
            "Turn-off did not switch the output off"
        )


@pytest.mark.asyncio
async def test_light_binary_zero_brightness_is_recoverable(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Zero brightness on an ON/OFF light must not leave it permanently stuck off.

    An ON/OFF light has no brightness capability, so `turn_on` with 0% brightness has
    no representable "on but dark" state. It must switch the output off and report the
    light as off, and a later plain turn-on must bring it back.
    """
    loop = asyncio.get_running_loop()
    pending: list[asyncio.Future[bool]] = []

    def on_log_line(line: str) -> None:
        if match := OUTPUT_PATTERN.search(line):
            value = match.group(1) == "YES"
            while pending:
                future = pending.pop(0)
                if not future.done():
                    future.set_result(value)
                    break

    def arm_output() -> asyncio.Future[bool]:
        future: asyncio.Future[bool] = loop.create_future()
        pending.append(future)
        return future

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

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        async def send_and_wait(timeout: float = 5.0, **kwargs: Any) -> LightState:
            state_futures[light.key] = loop.create_future()
            client.light_command(key=light.key, **kwargs)
            return await asyncio.wait_for(state_futures[light.key], timeout=timeout)

        output = arm_output()
        state = await send_and_wait(state=True)
        assert state.state is True
        assert await asyncio.wait_for(output, timeout=5.0) is True

        # Turning on at 0% brightness has no representable "on but dark" state here,
        # so the light must switch off and report itself as off.
        output = arm_output()
        state = await send_and_wait(state=True, brightness=0.0)
        assert await asyncio.wait_for(output, timeout=5.0) is False, (
            "Zero brightness did not switch the output off"
        )
        assert state.state is False, (
            "Light reported itself as on while its output was off"
        )

        # A plain turn-on must recover -- the stored zero brightness must not persist.
        output = arm_output()
        state = await send_and_wait(state=True)
        assert state.state is True
        assert await asyncio.wait_for(output, timeout=5.0) is True, (
            "Light was left permanently off by a zero-brightness turn-on"
        )

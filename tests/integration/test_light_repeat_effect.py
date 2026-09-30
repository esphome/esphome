"""Integration test for starting a strobe effect and for turn-ons sent while it runs.

Starting the strobe used to leave the last published colour (white here) on the output until
the first color's duration had passed since boot, then skip to the second color.

A turn-on naming the effect that is already running, or a plain turn-on of a lit light, used
to get the default transition (or the requested one). That faded the output towards white, which the strobe never
updates, until the next strobe step snapped it back.
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
async def test_light_repeat_effect(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The strobe starts on its first color, and turn-ons while it runs don't fade to white."""
    output_pattern = re.compile(r"(GREEN|BLUE)_OUTPUT:([\d.]+)")
    outputs: dict[str, list[float]] = {"GREEN": [], "BLUE": []}
    green = outputs["GREEN"]
    blue = outputs["BLUE"]
    warnings: list[str] = []

    def on_log_line(line: str) -> None:
        if match := output_pattern.search(line):
            outputs[match.group(1)].append(float(match.group(2)))
        elif "effect cannot be used with" in line:
            warnings.append(line)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        light = next(e for e in entities if e.object_id == "test_rgb_light")

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
            """Send a light command and wait for the matching state response."""
            state_futures[light.key] = asyncio.get_running_loop().create_future()
            client.light_command(key=light.key, **kwargs)
            return await asyncio.wait_for(state_futures[light.key], timeout=timeout)

        # Plain turn-on leaves the published colour at the white default
        state = await send_and_wait(state=True, transition_length=0.0)
        assert state.state is True

        green.clear()
        blue.clear()
        state = await send_and_wait(state=True, effect="Slow Strobe")
        assert state.effect == "Slow Strobe"
        await asyncio.sleep(0.3)

        # The first color is red; white shows green, and the second color is blue
        assert green and blue, "No output observed after starting the strobe"
        assert max(green) == pytest.approx(0.0, abs=0.01), (
            f"Starting the strobe showed white: green={green}"
        )
        assert max(blue) == pytest.approx(0.0, abs=0.01), (
            f"Starting the strobe skipped its first color: blue={blue}"
        )

        for description, command in (
            ("Repeating the running effect", {"effect": "Slow Strobe"}),
            (
                "Repeating the running effect with a transition",
                {"effect": "Slow Strobe", "transition_length": 2.0},
            ),
            ("A plain turn-on", {}),
        ):
            green.clear()
            state = await send_and_wait(state=True, **command)
            assert state.effect == "Slow Strobe", f"{description} changed the effect"
            # Cover the rest of the current strobe step and the start of the next one
            await asyncio.sleep(1.0)

            assert green, "No green output observed while the strobe was running"
            assert max(green) == pytest.approx(0.0, abs=0.01), (
                f"{description} faded the output towards white: {green}"
            )

        assert not warnings, f"Unexpected warnings: {warnings}"

        # A flash requested with the running effect still flashes, here to white
        green.clear()
        state = await send_and_wait(state=True, effect="Slow Strobe", flash_length=0.5)
        assert not warnings, f"Unexpected warnings: {warnings}"
        await asyncio.sleep(0.2)
        assert max(green) == pytest.approx(1.0, abs=0.01), (
            f"The flash was dropped: green={green}"
        )
        await asyncio.sleep(0.5)

        # Brightness 0 with no state turns the light off but leaves the effect
        # running; naming that effect must still turn the light on
        state = await send_and_wait(brightness=0.0)
        assert state.state is False
        state = await send_and_wait(state=True, effect="Slow Strobe")
        assert state.state is True, "Turn-on with the running effect was dropped"
        assert state.brightness == pytest.approx(1.0)
        assert state.effect == "Slow Strobe"

        # A lit light at brightness 0 is made visible by a turn-on naming the effect
        state = await send_and_wait(state=True, brightness=0.0)
        assert state.state is True
        assert state.brightness == pytest.approx(0.0)
        state = await send_and_wait(state=True, effect="Slow Strobe")
        assert state.brightness == pytest.approx(1.0), (
            "Turn-on with the running effect did not make the light visible"
        )
        assert state.effect == "Slow Strobe"

        # Turning the light off while naming the running effect must stop the effect, not just
        # publish the light as off and leave the effect driving the outputs
        green.clear()
        blue.clear()
        warnings.clear()
        state = await send_and_wait(state=False, effect="Slow Strobe")
        assert state.state is False
        await asyncio.sleep(2.2)  # A full strobe cycle
        assert max(green + blue) == pytest.approx(0.0, abs=0.01), (
            f"The effect kept driving the outputs after turn-off: green={green}, blue={blue}"
        )
        assert not warnings, f"Unexpected warnings: {warnings}"

        client.light_command(key=light.key, effect="None")

"""Test the homeassistant light platform against a fake Home Assistant."""

from __future__ import annotations

import asyncio
from collections.abc import Callable

from aioesphomeapi import (
    EntityState,
    HomeassistantServiceCall,
    LightColorCapability,
    LightState,
)
import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

ENTITY = "light.kitchen"
SUBSCRIPTIONS = {
    (ENTITY, None),
    (ENTITY, "supported_color_modes"),
    (ENTITY, "min_color_temp_kelvin"),
    (ENTITY, "max_color_temp_kelvin"),
    (ENTITY, "color_mode"),
    (ENTITY, "brightness"),
    (ENTITY, "color_temp_kelvin"),
    (ENTITY, "rgb_color"),
}
COLOR_TEMPERATURE = (
    LightColorCapability.ON_OFF
    | LightColorCapability.BRIGHTNESS
    | LightColorCapability.COLOR_TEMPERATURE
)
RGB = (
    LightColorCapability.ON_OFF
    | LightColorCapability.BRIGHTNESS
    | LightColorCapability.RGB
)


@pytest.mark.asyncio
async def test_api_homeassistant_light(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Imported attributes, echo suppression and the actions sent for local changes."""
    loop = asyncio.get_running_loop()
    waiter = LineWaiter()
    subscribed: set[tuple[str, str | None]] = set()
    all_subscribed = loop.create_future()
    calls: asyncio.Queue[HomeassistantServiceCall] = asyncio.Queue()
    states: asyncio.Queue[LightState] = asyncio.Queue()

    def on_state_sub(entity_id: str, attribute: str | None) -> None:
        subscribed.add((entity_id, attribute or None))
        if not all_subscribed.done() and subscribed >= SUBSCRIPTIONS:
            all_subscribed.set_result(None)

    def on_state(state: EntityState) -> None:
        if isinstance(state, LightState):
            states.put_nowait(state)

    async def next_call() -> HomeassistantServiceCall:
        return await asyncio.wait_for(calls.get(), timeout=5.0)

    async def next_state(matches: Callable[[LightState], bool]) -> LightState:
        async with asyncio.timeout(5.0):
            while not matches(state := await states.get()):
                pass
        return state

    def report(attribute: str | None, value: str) -> None:
        client.send_home_assistant_state(ENTITY, attribute, value)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, services = await client.list_entities_services()
        key = next(e.key for e in entities if e.name == "HA Light")
        set_force_off = next(s for s in services if s.name == "set_force_off")
        client.subscribe_service_calls(calls.put_nowait)
        client.subscribe_states(on_state)
        client.subscribe_home_assistant_states(on_state_sub)
        try:
            await asyncio.wait_for(all_subscribed, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"never subscribed: {SUBSCRIPTIONS - subscribed}")

        report("supported_color_modes", "['color_temp', 'rgb']")
        report("min_color_temp_kelvin", "2000")
        report("max_color_temp_kelvin", "6500")
        await waiter.wait_for("Supported color modes retrieved", timeout=5.0)
        # Normal while Home Assistant starts, so no warning
        report(None, "unavailable")
        report(None, "unknown")
        # Rejected, so the color stays unknown and the warnings name the attribute
        report("rgb_color", "(127.5, 64.0, 0.0)")
        report("rgb_color", "(0, 256, 0)")
        report("color_temp_kelvin", "0")
        await waiter.wait_for(
            "'color_temp_kelvin' value 0 is out of range", timeout=5.0
        )
        assert sum("Can't parse 'rgb_color'" in line for line in waiter.lines) == 2
        assert not any("as state!" in line for line in waiter.lines)

        # A report arrives as several attribute messages and is applied as one change
        report(None, "on")
        report("color_mode", "rgb")
        report("brightness", "128")
        report("rgb_color", "(255, 0, 0)")
        state = await next_state(lambda s: s.state)
        assert state.color_mode == RGB
        assert state.brightness == pytest.approx(128 / 255)
        assert (state.red, state.green, state.blue) == (1.0, 0.0, 0.0)
        # A rejected color leaves the reported one untouched
        report("rgb_color", "(0, 256, 0)")
        report("brightness", "255")
        state = await next_state(lambda s: s.brightness == 1.0)
        assert (state.red, state.green, state.blue) == (1.0, 0.0, 0.0)

        # The attributes of other modes stay with their mode, whatever order they arrive in
        report("color_mode", "color_temp")
        report("color_temp_kelvin", "4000")
        state = await next_state(lambda s: s.color_mode == COLOR_TEMPERATURE)
        assert state.color_temperature == 250.0
        report("rgb_color", "(0, 255, 0)")
        report("brightness", "200")
        state = await next_state(lambda s: s.brightness == pytest.approx(200 / 255))
        assert state.color_mode == COLOR_TEMPERATURE

        # Local changes go to Home Assistant with its attribute names and scales
        client.light_command(key, brightness=0.5)
        call = await next_call()
        assert call.service == "light.turn_on"
        assert call.data == {
            "entity_id": ENTITY,
            "brightness": "128",
            "color_temp_kelvin": "4000",
        }
        assert not call.data_template
        client.light_command(key, rgb=(0.0, 0.0, 1.0))
        call = await next_call()
        assert call.service == "light.turn_on"
        assert call.data == {"entity_id": ENTITY, "brightness": "128"}
        assert call.data_template == {"rgb_color": "[0,0,255]"}

        # Home Assistant echoes the result; that is not sent back. Returning to the state it
        # reported last is a local change and is sent.
        report(None, "off")
        report("brightness", "None")
        await next_state(lambda s: not s.state)
        await waiter.wait_for(
            "turned off", timeout=5.0
        )  # a reported state runs the triggers too
        client.light_command(key, state=True)
        assert (await next_call()).service == "light.turn_on"
        client.light_command(key, state=False)
        assert (await next_call()).service == "light.turn_off"

        # Without a report of the change within two seconds, the light goes back to the reported state
        report(None, "on")
        await next_state(lambda s: s.state)
        client.light_command(key, state=False)
        assert (await next_call()).service == "light.turn_off"
        await next_state(lambda s: s.state)
        await waiter.wait_for("did not report the change back", timeout=5.0)
        report(None, "off")
        await next_state(lambda s: not s.state)

        # An automation changing the light while a report is applied is a local change too
        await client.execute_service(set_force_off, {"value": True})
        report(None, "on")
        assert (await next_call()).service == "light.turn_off"
        await client.execute_service(set_force_off, {"value": False})
        report(None, "off")
        await next_state(lambda s: not s.state)

        # A change that leaves Home Assistant as it is gets no report, so none is sent
        client.light_command(key, state=False)
        await waiter.wait_for("already has this state", timeout=5.0)

        # A trait that arrives while a change waits for its report is not that report, and does
        # not bring the old state back early either
        client.light_command(key, state=True)
        assert (await next_call()).service == "light.turn_on"
        await next_state(lambda s: s.state)
        report("max_color_temp_kelvin", "6500")
        await next_state(lambda s: not s.state)
        assert (
            sum("did not report the change back" in line for line in waiter.lines) == 2
        )

        # A report that arrives during a flash is applied when the flash ends instead of being undone
        client.light_command(key, state=True, flash_length=1.0)
        report(None, "on")
        report("brightness", "77")
        state = await next_state(lambda s: s.brightness == pytest.approx(77 / 255))
        assert state.state
        await waiter.wait_for("arrived during the transition", timeout=5.0)

        # A trait or a repeated report during a local transition is nothing new, so the change the
        # transition ends at is still sent
        client.light_command(key, state=False, transition_length=1.0)
        report("max_color_temp_kelvin", "6500")
        report(None, "on")
        assert (await next_call()).service == "light.turn_off"
        report(None, "off")
        await next_state(lambda s: not s.state)

        # A newer local command wins over a report held during a transition
        client.light_command(key, state=True, transition_length=1.0)
        report("brightness", "50")
        await asyncio.sleep(0.1)  # held before the command arrives
        client.light_command(key, state=True, brightness=1.0)
        call = await next_call()
        assert call.service == "light.turn_on"
        assert call.data["brightness"] == "255"
        report(None, "on")
        report("brightness", "255")
        await next_state(lambda s: s.state and s.brightness == 1.0)

        # Also when it changes only the color, at the level the transition was heading for
        client.light_command(key, brightness=0.5, transition_length=1.0)
        report("brightness", "50")
        await asyncio.sleep(0.1)
        client.light_command(key, color_temperature=400.0)
        call = await next_call()
        assert call.service == "light.turn_on"
        assert call.data == {
            "entity_id": ENTITY,
            "brightness": "128",
            "color_temp_kelvin": "2500",
        }
        report("brightness", "128")
        report("color_temp_kelvin", "2500")
        await next_state(lambda s: s.color_temperature == 400.0)

        # Or when it is a transition itself, replacing the one the report was held for
        client.light_command(key, brightness=1.0, transition_length=1.0)
        report("brightness", "50")
        await asyncio.sleep(0.1)
        client.light_command(key, color_temperature=250.0, transition_length=1.0)
        call = await next_call()
        assert call.service == "light.turn_on"
        assert call.data == {
            "entity_id": ENTITY,
            "brightness": "255",
            "color_temp_kelvin": "4000",
        }
        report("brightness", "255")
        report("color_temp_kelvin", "4000")
        await next_state(lambda s: s.color_temperature == 250.0)

        # Or a transition back to where the one the report was held for started
        client.light_command(key, brightness=0.5, transition_length=1.0)
        report("brightness", "50")
        await asyncio.sleep(0.1)
        client.light_command(key, brightness=1.0, transition_length=1.0)
        call = await next_call()
        assert call.service == "light.turn_on"
        assert call.data["brightness"] == "255"
        report("brightness", "255")
        await next_state(lambda s: s.brightness == 1.0)

        # And when it goes back to the state the transition started from
        client.light_command(key, state=False)
        assert (await next_call()).service == "light.turn_off"
        report(None, "off")
        await next_state(lambda s: not s.state)
        client.light_command(key, state=True, transition_length=1.0)
        report(None, "on")
        report("brightness", "60")
        await asyncio.sleep(0.1)
        client.light_command(key, state=False)
        assert (await next_call()).service == "light.turn_off"
        report(None, "off")
        await next_state(lambda s: not s.state)

        # A flash ends where it began, which Home Assistant has, so nothing is sent and the user is told
        client.light_command(key, state=True, flash_length=0.5)
        await waiter.wait_for("a flash is never sent", timeout=5.0)

        assert calls.empty()

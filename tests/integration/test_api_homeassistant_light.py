"""Test the homeassistant light platform against a fake Home Assistant."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator, Callable
from contextlib import asynccontextmanager
from dataclasses import dataclass

from aioesphomeapi import (
    APIClient,
    EntityState,
    HomeassistantServiceCall,
    LightColorCapability,
    LightInfo,
    LightState,
    UserService,
)
from aioesphomeapi.api_pb2 import HomeAssistantStateResponse, LightCommandRequest
from google.protobuf import message
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
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


def command_msg(key: int, **fields: float | bool) -> LightCommandRequest:
    """A light command with has_<field> set for each field given."""
    return LightCommandRequest(
        key=key, **fields, **{f"has_{name}": True for name in fields}
    )


def report_msg(attribute: str, value: str) -> HomeAssistantStateResponse:
    return HomeAssistantStateResponse(
        entity_id=ENTITY, attribute=attribute, state=value
    )


@dataclass
class FakeHomeAssistant:
    """The light entity as the device sees it over the API, and what the device sends back."""

    client: APIClient
    key: int
    services: list[UserService]
    waiter: LineWaiter
    calls: asyncio.Queue[HomeassistantServiceCall]
    states: asyncio.Queue[LightState]

    async def next_call(self) -> HomeassistantServiceCall:
        return await asyncio.wait_for(self.calls.get(), timeout=5.0)

    async def expect_call(
        self, service: str = "light.turn_on", **data: str
    ) -> HomeassistantServiceCall:
        """The next action sent; its data is checked in full when given."""
        call = await self.next_call()
        assert call.service == service
        if data:
            assert call.data == {"entity_id": ENTITY, **data}
        return call

    def command(self, **kwargs: float | bool | tuple[float, float, float]) -> None:
        self.client.light_command(self.key, **kwargs)

    async def next_state(self, matches: Callable[[LightState], bool]) -> LightState:
        async with asyncio.timeout(5.0):
            while not matches(state := await self.states.get()):
                pass
        return state

    def report(self, attribute: str | None, value: str) -> None:
        self.client.send_home_assistant_state(ENTITY, attribute, value)

    async def report_off(self) -> None:
        self.report(None, "off")
        await self.next_state(lambda s: not s.state)

    def send_together(self, *messages: message.Message) -> None:
        """One write, so the device reads all of them in one loop pass."""
        self.client._get_connection().send_messages(messages)


@asynccontextmanager
async def light_entity(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    modes: str,
) -> AsyncIterator[FakeHomeAssistant]:
    """Boots the device and answers its subscriptions with a light of the given color modes."""
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

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, services = await client.list_entities_services()
        key = require_entity(entities, "ha_light", LightInfo).key
        client.subscribe_service_calls(calls.put_nowait)
        client.subscribe_states(on_state)
        client.subscribe_home_assistant_states(on_state_sub)
        try:
            await asyncio.wait_for(all_subscribed, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"never subscribed: {SUBSCRIPTIONS - subscribed}")
        ha = FakeHomeAssistant(client, key, services, waiter, calls, states)
        ha.report("supported_color_modes", modes)
        ha.report("min_color_temp_kelvin", "2000")
        ha.report("max_color_temp_kelvin", "6500")
        await waiter.wait_for("Supported color modes retrieved", timeout=5.0)
        yield ha


@pytest.mark.asyncio
async def test_api_homeassistant_light(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Imported attributes, echo suppression and the actions sent for local changes."""
    async with light_entity(
        yaml_config, run_compiled, api_client_connected, "['color_temp', 'rgb']"
    ) as ha:
        set_force_off = next(s for s in ha.services if s.name == "set_force_off")
        # Normal while Home Assistant starts, so no warning
        ha.report(None, "unavailable")
        ha.report(None, "unknown")
        # Rejected, so the color stays unknown and the warnings name the attribute
        ha.report("rgb_color", "(127.5, 64.0, 0.0)")
        ha.report("rgb_color", "(0, 256, 0)")
        ha.report("color_temp_kelvin", "0")
        await ha.waiter.wait_for(
            "'color_temp_kelvin' value 0 is out of range", timeout=5.0
        )
        assert sum("Can't parse 'rgb_color'" in line for line in ha.waiter.lines) == 2
        assert not any("as state!" in line for line in ha.waiter.lines)

        # A report arrives as several attribute messages and is applied as one change
        ha.report(None, "on")
        ha.report("color_mode", "rgb")
        ha.report("brightness", "128")
        ha.report("rgb_color", "(255, 0, 0)")
        state = await ha.next_state(lambda s: s.state)
        assert state.color_mode == RGB
        assert state.brightness == pytest.approx(128 / 255)
        assert (state.red, state.green, state.blue) == (1.0, 0.0, 0.0)
        # A rejected color leaves the reported one untouched
        ha.report("rgb_color", "(0, 256, 0)")
        ha.report("brightness", "255")
        state = await ha.next_state(lambda s: s.brightness == 1.0)
        assert (state.red, state.green, state.blue) == (1.0, 0.0, 0.0)

        # The attributes of other modes stay with their mode, whatever order they arrive in
        ha.report("color_mode", "color_temp")
        ha.report("color_temp_kelvin", "4000")
        state = await ha.next_state(lambda s: s.color_mode == COLOR_TEMPERATURE)
        assert state.color_temperature == 250.0
        ha.report("rgb_color", "(0, 255, 0)")
        ha.report("brightness", "200")
        state = await ha.next_state(lambda s: s.brightness == pytest.approx(200 / 255))
        assert state.color_mode == COLOR_TEMPERATURE

        # Local changes go to Home Assistant with its attribute names and scales; the color
        # temperature it reported is not repeated
        ha.command(brightness=0.5)
        call = await ha.expect_call(brightness="128")
        assert not call.data_template
        ha.command(rgb=(0.0, 0.0, 1.0))
        call = await ha.expect_call(brightness="128")
        assert call.data_template == {"rgb_color": "[0,0,255]"}

        # Home Assistant echoes the result; that is not sent back. Returning to the state it
        # reported last is a local change and is sent.
        ha.report(None, "off")
        ha.report("brightness", "None")
        await ha.next_state(lambda s: not s.state)
        await ha.waiter.wait_for(
            "turned off", timeout=5.0
        )  # a reported state runs the triggers too
        ha.command(state=True)
        await ha.expect_call()
        ha.command(state=False)
        await ha.expect_call("light.turn_off")

        # Without a report of the change within report_timeout, the light goes back to the reported state
        ha.report(None, "on")
        await ha.next_state(lambda s: s.state)
        ha.command(state=False)
        await ha.expect_call("light.turn_off")
        await ha.next_state(lambda s: s.state)
        await ha.waiter.wait_for("did not report the change back", timeout=5.0)
        await ha.report_off()

        # The fallback also drops the levels of the unsent change: the light keeps the brightness
        # Home Assistant reported last, which a later turn_on without a brightness would send
        ha.command(state=True, brightness=0.4)
        call = await ha.expect_call()
        assert call.data["brightness"] == "102"
        state = await ha.next_state(lambda s: not s.state)
        assert state.brightness == pytest.approx(200 / 255)

        # An automation changing the light while a report is applied is a local change too
        await ha.client.execute_service(set_force_off, {"value": True})
        ha.report(None, "on")
        await ha.expect_call("light.turn_off")
        await ha.client.execute_service(set_force_off, {"value": False})
        await ha.report_off()

        # A change that leaves Home Assistant as it is gets no report, so none is sent
        ha.command(state=False)
        await ha.waiter.wait_for("already has this state", timeout=5.0)

        # A trait that arrives while a change waits for its report is not that report, and does
        # not bring the old state back early either
        ha.command(state=True)
        await ha.expect_call()
        await ha.next_state(lambda s: s.state)
        ha.report("max_color_temp_kelvin", "6500")
        await ha.next_state(lambda s: not s.state)
        assert (
            sum("did not report the change back" in line for line in ha.waiter.lines)
            == 3
        )

        # A report that arrives during a flash is applied when the flash ends instead of being undone
        ha.command(state=True, flash_length=1.0)
        ha.report(None, "on")
        ha.report("brightness", "77")
        state = await ha.next_state(lambda s: s.brightness == pytest.approx(77 / 255))
        assert state.state
        await ha.waiter.wait_for("arrived during the transition", timeout=5.0)

        # A trait or a repeated report during a local transition is nothing new, so the change the
        # transition ends at is still sent
        ha.command(state=False, transition_length=1.0)
        ha.report("max_color_temp_kelvin", "6500")
        ha.report(None, "on")
        await ha.expect_call("light.turn_off")
        await ha.report_off()

        # A newer local command wins over a report held during a transition
        ha.command(state=True, transition_length=1.0)
        ha.report("brightness", "50")
        await asyncio.sleep(0.1)  # held before the command arrives
        ha.command(state=True, brightness=1.0)
        call = await ha.expect_call()
        assert call.data["brightness"] == "255"
        ha.report(None, "on")
        ha.report("brightness", "255")
        await ha.next_state(lambda s: s.state and s.brightness == 1.0)

        # Also when it changes only the color, at the level the transition was heading for
        ha.command(brightness=0.5, transition_length=1.0)
        ha.report("brightness", "50")
        await asyncio.sleep(0.1)
        ha.command(color_temperature=400.0)
        await ha.expect_call(brightness="128", color_temp_kelvin="2500")
        ha.report("brightness", "128")
        ha.report("color_temp_kelvin", "2500")
        await ha.next_state(lambda s: s.color_temperature == 400.0)

        # Or when it is a transition itself, replacing the one the report was held for
        ha.command(brightness=1.0, transition_length=1.0)
        ha.report("brightness", "50")
        await asyncio.sleep(0.1)
        ha.command(color_temperature=250.0, transition_length=1.0)
        await ha.expect_call(brightness="255", color_temp_kelvin="4000")
        ha.report("brightness", "255")
        ha.report("color_temp_kelvin", "4000")
        await ha.next_state(lambda s: s.color_temperature == 250.0)

        # Or a transition back to where the one the report was held for started
        ha.command(brightness=0.5, transition_length=1.0)
        ha.report("brightness", "50")
        await asyncio.sleep(0.1)
        ha.command(brightness=1.0, transition_length=1.0)
        call = await ha.expect_call()
        assert call.data["brightness"] == "255"
        ha.report("brightness", "255")
        await ha.next_state(lambda s: s.brightness == 1.0)

        # Or a transition to where the one the report was held for was already going
        ha.command(brightness=0.5, transition_length=1.0)
        ha.report("brightness", "50")
        await asyncio.sleep(0.1)
        ha.command(brightness=0.5, transition_length=1.0)
        call = await ha.expect_call()
        assert call.data["brightness"] == "128"
        ha.report("brightness", "128")
        await ha.next_state(lambda s: s.brightness == pytest.approx(128 / 255))

        # And when it goes back to the state the transition started from
        ha.command(state=False)
        await ha.expect_call("light.turn_off")
        await ha.report_off()
        ha.command(state=True, transition_length=1.0)
        ha.report(None, "on")
        ha.report("brightness", "60")
        await asyncio.sleep(0.1)
        ha.command(state=False)
        await ha.expect_call("light.turn_off")
        await ha.report_off()

        # Also when the report and the call that replaces the transition are read in the same loop
        # pass: the report is applied after the call, but it is older
        ha.send_together(
            command_msg(ha.key, state=True, brightness=0.5, transition_length=1000),
            report_msg("brightness", "50"),
            command_msg(ha.key, brightness=1.0, transition_length=1000),
        )
        call = await ha.expect_call()
        assert call.data["brightness"] == "255"
        ha.report(None, "on")
        ha.report("brightness", "255")
        await ha.next_state(lambda s: s.state and s.brightness == 1.0)

        # And a report read after that call in the same pass is newer than it: the hold follows the
        # last changed report, even when a trait opened the batch, so it is applied at the end
        ha.send_together(
            report_msg("max_color_temp_kelvin", "6500"),
            command_msg(ha.key, brightness=0.5, transition_length=1000),
            report_msg("brightness", "50"),
            command_msg(ha.key, brightness=0.8, transition_length=1000),
            report_msg("brightness", "77"),
        )
        state = await ha.next_state(lambda s: s.brightness == pytest.approx(77 / 255))
        assert state.state
        assert ha.calls.empty()
        ha.command(state=False)
        await ha.expect_call("light.turn_off")
        await ha.report_off()

        # A flash ends where it began, which Home Assistant has, so nothing is sent and the user is told
        ha.command(state=True, flash_length=0.5)
        await ha.waiter.wait_for("nothing to send (a flash never is)", timeout=5.0)

        assert ha.calls.empty()


@pytest.mark.asyncio
@pytest.mark.shared_yaml("api_homeassistant_light")
async def test_api_homeassistant_light_rgbww(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A cold/warm white light keeps its white balance: the color goes only when it changes."""
    async with light_entity(
        yaml_config, run_compiled, api_client_connected, "['color_temp', 'rgbww']"
    ) as ha:
        # Home Assistant reports rgb_color in every color mode, so the light is an RGB one here
        ha.report(None, "on")
        ha.report("color_mode", "rgbww")
        ha.report("brightness", "200")
        ha.report("rgb_color", "(255, 180, 120)")
        state = await ha.next_state(lambda s: s.state)
        assert state.color_mode == RGB
        assert state.brightness == pytest.approx(200 / 255)
        assert (state.red, state.green, state.blue) == pytest.approx(
            (1.0, 180 / 255, 120 / 255)
        )

        # A level change sends the level alone; Home Assistant derived the color it reports from
        # the white balance and would derive a different balance from the color sent back
        ha.command(brightness=0.5)
        call = await ha.expect_call(brightness="128")
        assert not call.data_template
        ha.report("brightness", "128")
        await ha.next_state(lambda s: s.brightness == pytest.approx(128 / 255))

        # A new color goes along
        ha.command(rgb=(0.0, 1.0, 0.0))
        call = await ha.expect_call(brightness="128")
        assert call.data_template == {"rgb_color": "[0,255,0]"}
        ha.report("rgb_color", "(0, 255, 0)")
        await ha.next_state(lambda s: s.green == 1.0 and s.red == 0.0)

        # Switching to color temperature sends it, and the level alone once there
        ha.command(color_temperature=250.0)
        call = await ha.expect_call(brightness="128", color_temp_kelvin="4000")
        assert not call.data_template
        ha.report("color_mode", "color_temp")
        ha.report("color_temp_kelvin", "4000")
        state = await ha.next_state(lambda s: s.color_mode == COLOR_TEMPERATURE)
        assert state.color_temperature == 250.0
        ha.command(brightness=1.0)
        await ha.expect_call(brightness="255")
        ha.report("brightness", "255")
        await ha.next_state(lambda s: s.brightness == 1.0)

        # Back to the color Home Assistant still reports: the mode changed, so the color is sent
        ha.command(rgb=(0.0, 1.0, 0.0))
        call = await ha.expect_call(brightness="255")
        assert call.data_template == {"rgb_color": "[0,255,0]"}
        ha.report("color_mode", "rgbww")
        await ha.next_state(lambda s: s.color_mode == RGB)

        assert ha.calls.empty()

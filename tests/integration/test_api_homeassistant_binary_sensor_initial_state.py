"""Test on_press/on_release for homeassistant binary sensors on the first HA state."""

from __future__ import annotations

import asyncio

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

ENTITIES = (
    "binary_sensor.initial_on",
    "binary_sensor.default",
    "binary_sensor.unavailable_first",
    "binary_sensor.initial_off",
    "binary_sensor.default_unavail",
)


@pytest.mark.asyncio
async def test_api_homeassistant_binary_sensor_initial_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The first state from HA fires on_press only with trigger_on_initial_state."""
    loop = asyncio.get_running_loop()
    waiter = LineWaiter()
    subscribed: set[str] = set()
    all_subscribed = loop.create_future()

    def on_state_sub(entity_id: str, _attribute: str | None) -> None:
        subscribed.add(entity_id)
        if not all_subscribed.done() and subscribed.issuperset(ENTITIES):
            all_subscribed.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        client.subscribe_home_assistant_states(on_state_sub)
        try:
            await asyncio.wait_for(all_subscribed, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"never subscribed: {set(ENTITIES) - subscribed}")

        # First state from HA
        client.send_home_assistant_state("binary_sensor.initial_on", "", "on")
        client.send_home_assistant_state("binary_sensor.default", "", "on")
        client.send_home_assistant_state(
            "binary_sensor.unavailable_first", "", "unavailable"
        )
        client.send_home_assistant_state("binary_sensor.unavailable_first", "", "on")
        client.send_home_assistant_state(
            "binary_sensor.default_unavail", "", "unavailable"
        )
        client.send_home_assistant_state("binary_sensor.default_unavail", "", "on")
        client.send_home_assistant_state("binary_sensor.initial_off", "", "off")

        await waiter.wait_for("initial_on on_press", timeout=5.0)
        await waiter.wait_for("unavailable_first on_press", timeout=5.0)
        # Pin that the 'unavailable' message actually arrived and was rejected
        await waiter.wait_for("Can't convert 'unavailable'", timeout=5.0)
        # initial_off is the last state sent, so this wait also proves the
        # earlier 'default' initial state was already processed
        await waiter.wait_for("initial_off on_release", timeout=5.0)
        # Both 'unavailable' senders must have been seen and rejected
        assert sum("Can't convert 'unavailable'" in line for line in waiter.lines) == 2
        # Guard every phase 2 needle against being satisfied by a stale
        # phase 1 line, and pin that the initial states fired nothing else
        for absent in (
            "initial_on on_release",
            "default on_press",
            "default on_release",
            "default_unavail on_press",
            "default_unavail on_release",
            "unavailable_first on_release",
            "initial_off on_press",
        ):
            assert not any(absent in line for line in waiter.lines), (
                f"unexpected trigger before the second state change: {absent}"
            )

        # A later change fires for all of them
        client.send_home_assistant_state("binary_sensor.initial_on", "", "off")
        client.send_home_assistant_state("binary_sensor.default", "", "off")
        client.send_home_assistant_state("binary_sensor.unavailable_first", "", "off")
        client.send_home_assistant_state("binary_sensor.initial_off", "", "on")
        client.send_home_assistant_state("binary_sensor.default_unavail", "", "off")
        await waiter.wait_for_each(
            "initial_on on_release",
            "default on_release",
            "default_unavail on_release",
            "unavailable_first on_release",
            "initial_off on_press",
            timeout=5.0,
        )

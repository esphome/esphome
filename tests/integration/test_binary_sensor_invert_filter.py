"""Integration test for the binary_sensor invert filter.

Verifies both condition forms the invert filter accepts:

1. A `!lambda` condition (``TemplatableFn<bool>`` storage path).
2. A native automation condition, ``switch.is_on`` (``Condition<>*`` storage
   path), which can be toggled at runtime to enable/disable the inversion.

Each button press invalidates the sensor before republishing, so every press
is guaranteed to produce a fresh state event over the API regardless of
whether the filtered output happens to repeat a prior value.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorState, EntityState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_binary_sensor_invert_filter(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    loop = asyncio.get_running_loop()
    switch_states: list[bool] = []
    lambda_states: list[bool] = []
    waiters: list[tuple[int, int, asyncio.Future[bool]]] = []

    def check_waiters() -> None:
        for switch_count, lambda_count, future in waiters:
            if (
                not future.done()
                and len(switch_states) >= switch_count
                and len(lambda_states) >= lambda_count
            ):
                future.set_result(True)

    def on_state(state: EntityState) -> None:
        if not isinstance(state, BinarySensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if sensor_name == "invert_switch_sensor":
            switch_states.append(state.state)
        elif sensor_name == "invert_lambda_sensor":
            lambda_states.append(state.state)
        check_waiters()

    async def wait_for_counts(switch_count: int, lambda_count: int) -> None:
        future = loop.create_future()
        waiters.append((switch_count, lambda_count, future))
        check_waiters()
        try:
            await asyncio.wait_for(future, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for states. "
                f"switch={switch_states} lambda={lambda_states}"
            )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "test-invert-filter"

        entities, _ = await client.list_entities_services()
        key_to_sensor = build_key_to_entity_mapping(
            entities, ["invert_switch_sensor", "invert_lambda_sensor"]
        )

        publish_true = require_entity(
            entities, "publish_true", description="Publish True button"
        )
        publish_false = require_entity(
            entities, "publish_false", description="Publish False button"
        )
        enable_invert = require_entity(
            entities, "enable_invert", description="Enable Invert button"
        )

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # === Condition off: filter passes values through unchanged ===
        client.button_command(publish_true.key)
        await wait_for_counts(1, 1)
        assert switch_states[-1] is True, switch_states
        assert lambda_states[-1] is True, lambda_states

        client.button_command(publish_false.key)
        await wait_for_counts(2, 2)
        assert switch_states[-1] is False, switch_states
        assert lambda_states[-1] is False, lambda_states

        # === Enable the condition: filter now inverts ===
        client.button_command(enable_invert.key)

        client.button_command(publish_true.key)
        await wait_for_counts(3, 3)
        assert switch_states[-1] is False, (
            f"switch.is_on condition should invert published True to False, got {switch_states}"
        )
        assert lambda_states[-1] is False, (
            f"lambda condition should invert published True to False, got {lambda_states}"
        )

        client.button_command(publish_false.key)
        await wait_for_counts(4, 4)
        assert switch_states[-1] is True, (
            f"switch.is_on condition should invert published False to True, got {switch_states}"
        )
        assert lambda_states[-1] is True, (
            f"lambda condition should invert published False to True, got {lambda_states}"
        )

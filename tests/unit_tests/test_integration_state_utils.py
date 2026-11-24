"""Tests for the lookup helpers in tests/integration/state_utils.py."""

from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, MagicMock

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    ButtonInfo,
    DeviceInfo,
    EntityInfo,
    LightInfo,
    SensorInfo,
    SensorState,
    UserService,
)
import pytest

from tests.integration.state_utils import (
    InitialStateHelper,
    SensorStateCollector,
    assert_device_name,
    find_entity,
    require_entity,
    require_only_entity,
    require_service,
    wait_for_state,
)

ENTITIES: list[EntityInfo] = [
    SensorInfo(object_id="temperature_raw", key=1),
    SensorInfo(object_id="temperature", key=2),
    BinarySensorInfo(object_id="Motion", key=3),
    ButtonInfo(object_id="restart", key=4),
]


def test_find_entity_substring_default() -> None:
    assert find_entity(ENTITIES, "temperature").key == 1
    assert find_entity(ENTITIES, "motion").key == 3


def test_find_entity_exact() -> None:
    assert find_entity(ENTITIES, "temperature", exact=True).key == 2
    assert find_entity(ENTITIES, "temp", exact=True) is None
    assert find_entity(ENTITIES, "motion", exact=True) is None
    assert find_entity(ENTITIES, "temperature", ButtonInfo, exact=True) is None


def test_require_entity_exact() -> None:
    assert require_entity(ENTITIES, "Motion", BinarySensorInfo, exact=True).key == 3
    with pytest.raises(AssertionError, match="object_id 'temp' of type SensorInfo"):
        require_entity(ENTITIES, "temp", SensorInfo, exact=True)


def test_require_only_entity() -> None:
    assert require_only_entity(ENTITIES, ButtonInfo).key == 4
    with pytest.raises(AssertionError, match="exactly one SensorInfo"):
        require_only_entity(ENTITIES, SensorInfo)
    with pytest.raises(AssertionError, match=r"exactly one LightInfo, found \[\]"):
        require_only_entity(ENTITIES, LightInfo)


def test_require_service() -> None:
    services = [UserService(name="set_level", key=1), UserService(name="set", key=2)]
    assert require_service(services, "set").key == 2
    with pytest.raises(AssertionError, match=r"'missing' not found.*set_level"):
        require_service(services, "missing")


@pytest.mark.asyncio
async def test_assert_device_name() -> None:
    info = DeviceInfo(name="host-test")
    client = AsyncMock()
    client.device_info.return_value = info
    assert await assert_device_name(client, "host-test") is info
    with pytest.raises(AssertionError, match="expected device name 'other'"):
        await assert_device_name(client, "other")
    client.device_info.return_value = None
    with pytest.raises(AssertionError, match="returned None"):
        await assert_device_name(client, "host-test")


@pytest.mark.asyncio
async def test_wait_for_initial_states_fails_naming_missing_entities() -> None:
    helper = InitialStateHelper(ENTITIES)
    wrapper = helper.on_state_wrapper(lambda state: None)
    wrapper(SensorState(key=1, state=1.0))
    wrapper(BinarySensorState(key=3, state=True))
    with pytest.raises(pytest.fail.Exception) as err:
        await helper.wait_for_initial_states(timeout=0.01)
    assert str(err.value) == (
        "Timed out waiting for initial states (after 0.01s)\n"
        "still_waiting (last 1 of 1):\n  SensorInfo temperature"
    )


@pytest.mark.asyncio
async def test_wait_for_all_fails_naming_missing_sensors() -> None:
    collector = SensorStateCollector(
        ["temperature_raw", "temperature"], ["Motion"], entities=ENTITIES
    )
    collector.on_state(SensorState(key=1, state=1.5))
    with pytest.raises(pytest.fail.Exception) as err:
        await collector.wait_for_all(timeout=0.01)
    message = str(err.value)
    assert message.startswith("Timed out waiting for every sensor to report")
    assert "still_waiting (last 2 of 2):\n  temperature\n  Motion" in message
    assert message.endswith("received (last 1 of 1):\n  temperature_raw: [1.5]")


@pytest.mark.asyncio
async def test_wait_for_state_fails_listing_received_states() -> None:
    client = MagicMock()
    waiting = asyncio.ensure_future(
        wait_for_state(client, lambda state: state.state == 2.0, 0.01, "no 2.0")
    )
    await asyncio.sleep(0)
    on_state = client.subscribe_states.call_args.args[0]
    on_state(SensorState(key=1, state=1.0))
    with pytest.raises(pytest.fail.Exception) as err:
        await waiting
    message = str(err.value)
    assert message.startswith("no 2.0 (after 0.01s)\nreceived (last 1 of 1):")
    assert "state=1.0" in message
    on_state(SensorState(key=1, state=2.0))

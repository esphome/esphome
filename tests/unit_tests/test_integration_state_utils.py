"""Tests for the lookup helpers in tests/integration/state_utils.py."""

from __future__ import annotations

from unittest.mock import AsyncMock

from aioesphomeapi import (
    BinarySensorInfo,
    ButtonInfo,
    DeviceInfo,
    EntityInfo,
    LightInfo,
    SensorInfo,
    UserService,
)
import pytest

from tests.integration.state_utils import (
    assert_device_name,
    find_entity,
    require_entity,
    require_only_entity,
    require_service,
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

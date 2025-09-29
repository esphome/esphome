from unittest.mock import Mock

import pytest
from types import SimpleNamespace

from esphome import const, cpp_helpers as ch


@pytest.mark.asyncio
async def test_gpio_pin_expression__conf_is_none(monkeypatch):
    actual = await ch.gpio_pin_expression(None)

    assert actual is None


@pytest.mark.asyncio
async def test_register_component(monkeypatch):
    var = Mock(base="foo.bar")

    app_mock = Mock(register_component=Mock(return_value=var))
    monkeypatch.setattr(ch, "App", app_mock)

    core_mock = Mock(component_ids=["foo.bar"])
    monkeypatch.setattr(ch, "CORE", core_mock)

    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)

    actual = await ch.register_component(var, {})

    assert actual is var
    assert add_mock.call_count == 2
    app_mock.register_component.assert_called_with(var)
    assert core_mock.component_ids == []


@pytest.mark.asyncio
async def test_register_component__no_component_id(monkeypatch):
    var = Mock(base="foo.eek")

    core_mock = Mock(component_ids=["foo.bar"])
    monkeypatch.setattr(ch, "CORE", core_mock)

    with pytest.raises(ValueError, match="Component ID foo.eek was not declared to"):
        await ch.register_component(var, {})


@pytest.mark.asyncio
async def test_register_component__with_setup_priority(monkeypatch):
    var = Mock(base="foo.bar")

    app_mock = Mock(register_component=Mock(return_value=var))
    monkeypatch.setattr(ch, "App", app_mock)

    core_mock = Mock(component_ids=["foo.bar"])
    monkeypatch.setattr(ch, "CORE", core_mock)

    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)

    actual = await ch.register_component(
        var,
        {
            const.CONF_SETUP_PRIORITY: "123",
            const.CONF_UPDATE_INTERVAL: "456",
        },
    )

    assert actual is var
    add_mock.assert_called()
    assert add_mock.call_count == 4
    app_mock.register_component.assert_called_with(var)
    assert core_mock.component_ids == []


@pytest.mark.asyncio
async def test_setup_entity_default_topic_from_name(monkeypatch):
    var = Mock()
    monkeypatch.setattr(ch, "add", Mock())
    core_mock = SimpleNamespace(
        config={const.CONF_MQTT: {}},
        friendly_name="Friendly Name",
    )
    monkeypatch.setattr(ch, "CORE", core_mock)

    config = {
        const.CONF_NAME: "Мой Датчик",
        const.CONF_DISABLED_BY_DEFAULT: False,
    }

    await ch.setup_entity(var, config)

    var.set_object_id.assert_called_once_with("мой_датчик")


@pytest.mark.asyncio
async def test_setup_entity_topic_from_id(monkeypatch):
    var = Mock()
    monkeypatch.setattr(ch, "add", Mock())
    core_mock = SimpleNamespace(
        config={
            const.CONF_MQTT: {
                const.CONF_TOPIC_NAME_SOURCE: const.TOPIC_NAME_SOURCE_ID,
            }
        },
        friendly_name="Friendly Name",
    )
    monkeypatch.setattr(ch, "CORE", core_mock)

    config = {
        const.CONF_NAME: "My Sensor",
        const.CONF_ID: SimpleNamespace(id="sensor_id"),
        const.CONF_DISABLED_BY_DEFAULT: False,
    }

    await ch.setup_entity(var, config)

    var.set_object_id.assert_called_once_with("sensor_id")


@pytest.mark.asyncio
async def test_setup_entity_topic_from_unicode_id(monkeypatch):
    var = Mock()
    monkeypatch.setattr(ch, "add", Mock())
    core_mock = SimpleNamespace(
        config={
            const.CONF_MQTT: {
                const.CONF_TOPIC_NAME_SOURCE: const.TOPIC_NAME_SOURCE_ID,
            }
        },
        friendly_name="Friendly Name",
    )
    monkeypatch.setattr(ch, "CORE", core_mock)

    config = {
        const.CONF_NAME: "My Sensor",
        const.CONF_ID: SimpleNamespace(id="датчик_1"),
        const.CONF_DISABLED_BY_DEFAULT: False,
    }

    await ch.setup_entity(var, config)

    var.set_object_id.assert_called_once_with("датчик_1")


@pytest.mark.asyncio
async def test_setup_entity_topic_from_id_without_id_falls_back_to_name(monkeypatch):
    var = Mock()
    monkeypatch.setattr(ch, "add", Mock())
    core_mock = SimpleNamespace(
        config={
            const.CONF_MQTT: {
                const.CONF_TOPIC_NAME_SOURCE: const.TOPIC_NAME_SOURCE_ID,
            }
        },
        friendly_name="Friendly Name",
    )
    monkeypatch.setattr(ch, "CORE", core_mock)

    config = {
        const.CONF_NAME: "My Sensor",
        const.CONF_DISABLED_BY_DEFAULT: False,
    }

    await ch.setup_entity(var, config)

    var.set_object_id.assert_called_once_with("my_sensor")


@pytest.mark.asyncio
async def test_setup_entity_uses_core_friendly_name_when_name_empty(monkeypatch):
    var = Mock()
    monkeypatch.setattr(ch, "add", Mock())
    core_mock = SimpleNamespace(
        config={const.CONF_MQTT: {}},
        friendly_name="Гостиная Датчик",
    )
    monkeypatch.setattr(ch, "CORE", core_mock)

    config = {
        const.CONF_NAME: "",
        const.CONF_DISABLED_BY_DEFAULT: False,
    }

    await ch.setup_entity(var, config)

    var.set_object_id.assert_called_once_with("гостиная_датчик")

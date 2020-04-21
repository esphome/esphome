import pytest
from mock import Mock

from esphome import cpp_helpers as ch
from esphome import const
from esphome.cpp_generator import MockObj


def test_gpio_pin_expression__conf_is_none(monkeypatch):
    target = ch.gpio_pin_expression(None)

    actual = next(target)

    assert actual is None


def test_gpio_pin_expression__new_pin(monkeypatch):
    target = ch.gpio_pin_expression({
        const.CONF_NUMBER: 42,
        const.CONF_MODE: "input",
        const.CONF_INVERTED: False
    })

    actual = next(target)

    assert isinstance(actual, MockObj)


def test_register_component(monkeypatch):
    var = Mock(base="foo.bar")

    app_mock = Mock(register_component=Mock(return_value=var))
    monkeypatch.setattr(ch, "App", app_mock)

    core_mock = Mock(component_ids=["foo.bar"])
    monkeypatch.setattr(ch, "CORE", core_mock)

    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)

    target = ch.register_component(var, {})

    actual = next(target)

    assert actual is var
    add_mock.assert_called_once()
    app_mock.register_component.assert_called_with(var)
    assert core_mock.component_ids == []


def test_register_component__no_component_id(monkeypatch):
    var = Mock(base="foo.eek")

    core_mock = Mock(component_ids=["foo.bar"])
    monkeypatch.setattr(ch, "CORE", core_mock)

    with pytest.raises(ValueError, match="Component ID foo.eek was not declared to"):
        target = ch.register_component(var, {})
        next(target)


def test_register_component__with_setup_priority(monkeypatch):
    var = Mock(base="foo.bar")

    app_mock = Mock(register_component=Mock(return_value=var))
    monkeypatch.setattr(ch, "App", app_mock)

    core_mock = Mock(component_ids=["foo.bar"])
    monkeypatch.setattr(ch, "CORE", core_mock)

    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)

    target = ch.register_component(var, {
        const.CONF_SETUP_PRIORITY: "123",
        const.CONF_UPDATE_INTERVAL: "456",
    })

    actual = next(target)

    assert actual is var
    add_mock.assert_called()
    assert add_mock.call_count == 3
    app_mock.register_component.assert_called_with(var)
    assert core_mock.component_ids == []

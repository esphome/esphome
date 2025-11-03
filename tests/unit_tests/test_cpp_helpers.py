from unittest.mock import Mock

import pytest

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


def test_require_wake_loop_threadsafe__first_call() -> None:
    """Test that first call sets up define and consumes socket."""
    ch.require_wake_loop_threadsafe()

    # Verify CORE.data was updated
    assert ch.CORE.data[ch.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Verify the define was added
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in ch.CORE.defines)


def test_require_wake_loop_threadsafe__idempotent() -> None:
    """Test that subsequent calls are idempotent."""
    # Set up initial state as if already called
    ch.CORE.data[ch.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] = True

    # Call again - should not raise or fail
    ch.require_wake_loop_threadsafe()

    # Verify state is still True
    assert ch.CORE.data[ch.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Define should not be added since flag was already True
    assert not any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in ch.CORE.defines)


def test_require_wake_loop_threadsafe__multiple_calls() -> None:
    """Test that multiple calls only set up once."""
    # Call three times
    ch.require_wake_loop_threadsafe()
    ch.require_wake_loop_threadsafe()
    ch.require_wake_loop_threadsafe()

    # Verify CORE.data was set
    assert ch.CORE.data[ch.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Verify the define was added (only once, but we can just check it exists)
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in ch.CORE.defines)

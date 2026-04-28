import logging
from unittest.mock import Mock

import pytest

from esphome import const, cpp_helpers as ch
from esphome.cpp_helpers import ComponentSourcePool, register_component_source


@pytest.mark.asyncio
async def test_gpio_pin_expression__conf_is_none(monkeypatch):
    actual = await ch.gpio_pin_expression(None)

    assert actual is None


@pytest.mark.asyncio
async def test_register_component(monkeypatch):
    base_mock = Mock()
    base_mock.__str__ = lambda self: "foo.bar"
    base_mock.type = Mock()
    base_mock.type.__str__ = lambda self: "foo::Bar"
    var = Mock(base=base_mock)

    app_mock = Mock(register_component_=Mock(return_value=var))
    monkeypatch.setattr(ch, "App", app_mock)

    core_mock = Mock(component_ids=["foo.bar"], data={})
    monkeypatch.setattr(ch, "CORE", core_mock)

    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)

    actual = await ch.register_component(var, {})

    assert actual is var
    assert add_mock.call_count == 1
    app_mock.register_component_.assert_called_once()
    assert app_mock.register_component_.call_args.args[0] is var
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
    base_mock = Mock()
    base_mock.__str__ = lambda self: "foo.bar"
    base_mock.type = Mock()
    base_mock.type.__str__ = lambda self: "foo::Bar"
    var = Mock(base=base_mock)

    app_mock = Mock(register_component_=Mock(return_value=var))
    monkeypatch.setattr(ch, "App", app_mock)

    core_mock = Mock(component_ids=["foo.bar"], data={})
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
    assert add_mock.call_count == 3
    app_mock.register_component_.assert_called_once()
    assert app_mock.register_component_.call_args.args[0] is var
    assert core_mock.component_ids == []


def test_register_component_source_empty_name(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(ch, "CORE", Mock(data={}))
    assert register_component_source("") == 0


def test_register_component_source_deduplicates(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(ch, "CORE", Mock(data={}))
    idx1 = register_component_source("wifi")
    idx2 = register_component_source("api")
    idx3 = register_component_source("wifi")
    assert idx1 == 1
    assert idx2 == 2
    assert idx3 == 1  # deduplicated


def test_generate_source_table_code_empty() -> None:
    from esphome.cpp_helpers import _generate_source_table_code

    assert _generate_source_table_code("TBL", "lookup", {}) == ""


def test_generate_source_table_code_non_empty() -> None:
    from esphome.cpp_helpers import _generate_source_table_code

    code = _generate_source_table_code("TBL", "lookup", {"wifi": 1, "api": 2})
    assert "PROGMEM" in code
    assert "wifi" in code
    assert "api" in code
    assert "lookup" in code
    assert "index == 0" in code
    assert "progmem_read_ptr" in code
    assert "index > 2" in code


@pytest.mark.asyncio
async def test_generate_component_source_table_empty_pool(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test that _generate_component_source_table does nothing with an empty pool."""
    from esphome.cpp_helpers import _generate_component_source_table

    monkeypatch.setattr(ch, "CORE", Mock(data={}))
    add_global_mock = Mock()
    monkeypatch.setattr(ch, "add_global", add_global_mock)
    await _generate_component_source_table()
    add_global_mock.assert_not_called()


def test_register_component_source_overflow_warns(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    # Pre-fill pool to max
    pool = ComponentSourcePool(
        sources={f"comp_{i}": i + 1 for i in range(0xFF)},
        table_registered=True,
    )
    monkeypatch.setattr(
        ch, "CORE", Mock(data={ch._COMPONENT_SOURCE_DOMAIN: pool}, testing_mode=False)
    )
    with caplog.at_level(logging.WARNING):
        idx = register_component_source("overflow_component")
    assert idx == 0
    assert "Too many unique component source names" in caplog.text
    assert "overflow_component" in caplog.text


def test_register_component_source_overflow_suppressed_in_testing_mode(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    # Pre-fill pool to max
    pool = ComponentSourcePool(
        sources={f"comp_{i}": i + 1 for i in range(0xFF)},
        table_registered=True,
    )
    monkeypatch.setattr(
        ch, "CORE", Mock(data={ch._COMPONENT_SOURCE_DOMAIN: pool}, testing_mode=True)
    )
    with caplog.at_level(logging.WARNING):
        idx = register_component_source("overflow_component")
    assert idx == 0
    assert "Too many unique component source names" not in caplog.text

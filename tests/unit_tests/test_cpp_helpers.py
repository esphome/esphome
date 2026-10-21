from collections.abc import Awaitable, Callable
import importlib.util
import logging
from pathlib import Path
import sys
from types import ModuleType
from typing import Any
from unittest.mock import AsyncMock, Mock

import pytest

from esphome import const, cpp_helpers as ch
from esphome.core import CoroPriority, coroutine_with_priority
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


def _define_value(name: str) -> str | None:
    for define in ch.CORE.defines:
        if define.name == name:
            # Values are codegen expressions (IntLiteral); compare rendered.
            return str(define.value)
    return None


def test_slot_counter_emits_requested_count() -> None:
    """Each request bumps the count; the self-scheduled FINAL job emits it."""
    request = ch.slot_counter("TEST_SLOT_COUNT")
    request()
    request()
    ch.CORE.flush_tasks()
    assert _define_value("TEST_SLOT_COUNT") == "2"


def test_slot_counter_keyed_emits_largest_count() -> None:
    """Keyed requests size storage every key declares at the same capacity:
    the define is the busiest key's count, not the total over all keys."""
    request = ch.slot_counter("TEST_SLOT_COUNT_KEYED")
    request("rx_a")
    request("rx_a")
    request("rx_a")
    request("rx_b")
    assert ch.get_slot_count("TEST_SLOT_COUNT_KEYED") == 3
    ch.CORE.flush_tasks()
    assert _define_value("TEST_SLOT_COUNT_KEYED") == "3"


def test_slot_counter_rejects_mixed_keyed_and_unkeyed_requests() -> None:
    """A keyed and an unkeyed request for one define cannot be sized together."""
    request = ch.slot_counter("TEST_SLOT_COUNT_MIXED")
    request("rx_a")
    with pytest.raises(ValueError, match="TEST_SLOT_COUNT_MIXED"):
        request()
    unkeyed = ch.slot_counter("TEST_SLOT_COUNT_MIXED_2")
    unkeyed()
    with pytest.raises(ValueError, match="TEST_SLOT_COUNT_MIXED_2"):
        unkeyed("rx_a")


def test_slot_counter_without_requests_emits_nothing() -> None:
    """No requests, no job, no define — the guarded storage compiles out."""
    ch.slot_counter("TEST_SLOT_COUNT_UNUSED")
    ch.CORE.flush_tasks()
    assert _define_value("TEST_SLOT_COUNT_UNUSED") is None


def test_slot_counter_request_from_final_job_still_emits() -> None:
    """The FIRST request for a define may come from a FINAL job: its emit job
    is scheduled mid-drain and flush_tasks() loops until the heap is empty.
    Later requests do not get this guarantee — see the companion test."""
    request = ch.slot_counter("TEST_SLOT_COUNT_LATE")

    @coroutine_with_priority(CoroPriority.FINAL)
    async def late_requester() -> None:
        request()

    ch.CORE.add_job(late_requester)
    ch.CORE.flush_tasks()
    assert _define_value("TEST_SLOT_COUNT_LATE") == "1"


def test_slot_counter_request_after_emit_raises() -> None:
    """The boundary of FINAL-time requests: once the define was emitted, a
    further request would silently undersize the storage, so it fails loudly."""
    request = ch.slot_counter("TEST_SLOT_COUNT_TOO_LATE")
    request()
    ch.CORE.flush_tasks()
    assert _define_value("TEST_SLOT_COUNT_TOO_LATE") == "1"
    with pytest.raises(ValueError, match="TEST_SLOT_COUNT_TOO_LATE"):
        request()


def _load_module(
    monkeypatch: pytest.MonkeyPatch, name: str, path: Path, source: str
) -> ModuleType:
    path.write_text(source, encoding="utf-8")
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    monkeypatch.setitem(sys.modules, name, module)
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize(
    ("wrapper", "expected"),
    [
        # A plain wrapper returns the coroutine, so its frame is gone when
        # register_component looks for the source; an async one is still on the stack.
        ("def new_component(config):\n    return ", "fake_dev"),
        ("async def new_component(config):\n    return await ", "fake_bus"),
    ],
)
@pytest.mark.asyncio
async def test_new_bus_component_source_is_the_caller(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, wrapper: str, expected: str
) -> None:
    bus = _load_module(
        monkeypatch,
        "esphome.components.fake_bus",
        tmp_path / "fake_bus.py",
        "from esphome import cpp_helpers as ch\n"
        "async def register_device(var, config):\n"
        "    pass\n"
        f"{wrapper}ch.new_bus_component(register_device, config)\n",
    )
    dev = _load_module(
        monkeypatch,
        "esphome.components.fake_dev",
        tmp_path / "fake_dev.py",
        "async def to_code(new_component, config):\n"
        "    return await new_component(config)\n",
    )
    base = Mock()
    base.__str__ = lambda self: "foo.bar"
    var = Mock(base=base)
    monkeypatch.setattr(ch, "new_Pvariable", Mock(return_value=var))
    monkeypatch.setattr(ch, "CORE", Mock(component_ids=["foo.bar"], data={}))
    monkeypatch.setattr(ch, "add", Mock())
    monkeypatch.setattr(ch, "App", Mock())
    source = Mock(return_value=1)
    monkeypatch.setattr(ch, "register_component_source", source)
    result = await dev.to_code(bus.new_component, {const.CONF_ID: "foo.bar"})
    assert result is var
    source.assert_called_once_with(expected)


@pytest.mark.asyncio
@pytest.mark.parametrize("helper", [ch.add_pin, ch.add_variable])
async def test_add_helpers_skip_none(
    monkeypatch: pytest.MonkeyPatch, helper: Callable[..., Awaitable[Any]]
) -> None:
    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)
    setter = Mock()

    assert await helper(setter, None) is None
    setter.assert_not_called()
    add_mock.assert_not_called()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("helper", "lookup"),
    [("add_pin", "gpio_pin_expression"), ("add_variable", "get_variable")],
)
async def test_add_helpers_pass_value_to_setter(
    monkeypatch: pytest.MonkeyPatch, helper: str, lookup: str
) -> None:
    value = Mock()
    lookup_mock = AsyncMock(return_value=value)
    monkeypatch.setattr(ch, lookup, lookup_mock)
    add_mock = Mock()
    monkeypatch.setattr(ch, "add", add_mock)
    setter = Mock()
    arg = Mock()

    assert await getattr(ch, helper)(setter, arg) is value
    lookup_mock.assert_awaited_once_with(arg)
    setter.assert_called_once_with(value)
    add_mock.assert_called_once_with(setter.return_value)

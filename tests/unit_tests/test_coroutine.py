"""Tests for the coroutine module."""

import pytest

from esphome.coroutine import CoroPriority, FakeEventLoop, coroutine_with_priority


def test_coro_priority_enum_values() -> None:
    """Test that CoroPriority enum values match expected priorities."""
    assert CoroPriority.PLATFORM == 1000
    assert CoroPriority.NETWORK == 201
    assert CoroPriority.NETWORK_TRANSPORT == 200
    assert CoroPriority.CORE == 100
    assert CoroPriority.DIAGNOSTICS == 90
    assert CoroPriority.STATUS == 80
    assert CoroPriority.COMMUNICATION == 60
    assert CoroPriority.APPLICATION == 50
    assert CoroPriority.WEB == 40
    assert CoroPriority.AUTOMATION == 30
    assert CoroPriority.BUS == 1
    assert CoroPriority.COMPONENT == 0
    assert CoroPriority.LATE == -100
    assert CoroPriority.WORKAROUNDS == -999
    assert CoroPriority.FINAL == -1000


def test_coroutine_with_priority_accepts_float() -> None:
    """Test that coroutine_with_priority accepts float values."""

    @coroutine_with_priority(100.0)
    def test_func() -> None:
        pass

    assert hasattr(test_func, "priority")
    assert test_func.priority == 100.0


def test_coroutine_with_priority_accepts_enum() -> None:
    """Test that coroutine_with_priority accepts CoroPriority enum values."""

    @coroutine_with_priority(CoroPriority.CORE)
    def test_func() -> None:
        pass

    assert hasattr(test_func, "priority")
    assert test_func.priority == 100.0


def test_float_and_enum_are_interchangeable() -> None:
    """Test that float and CoroPriority enum values produce the same priority."""

    @coroutine_with_priority(100.0)
    def func_with_float() -> None:
        pass

    @coroutine_with_priority(CoroPriority.CORE)
    def func_with_enum() -> None:
        pass

    assert func_with_float.priority == func_with_enum.priority
    assert func_with_float.priority == 100.0


@pytest.mark.parametrize(
    ("enum_value", "float_value"),
    [
        (CoroPriority.PLATFORM, 1000.0),
        (CoroPriority.NETWORK, 201.0),
        (CoroPriority.NETWORK_TRANSPORT, 200.0),
        (CoroPriority.CORE, 100.0),
        (CoroPriority.DIAGNOSTICS, 90.0),
        (CoroPriority.STATUS, 80.0),
        (CoroPriority.COMMUNICATION, 60.0),
        (CoroPriority.APPLICATION, 50.0),
        (CoroPriority.WEB, 40.0),
        (CoroPriority.AUTOMATION, 30.0),
        (CoroPriority.BUS, 1.0),
        (CoroPriority.COMPONENT, 0.0),
        (CoroPriority.LATE, -100.0),
        (CoroPriority.WORKAROUNDS, -999.0),
        (CoroPriority.FINAL, -1000.0),
    ],
)
def test_all_priority_values_are_interchangeable(
    enum_value: CoroPriority, float_value: float
) -> None:
    """Test that all CoroPriority values work correctly with coroutine_with_priority."""

    @coroutine_with_priority(enum_value)
    def func_with_enum() -> None:
        pass

    @coroutine_with_priority(float_value)
    def func_with_float() -> None:
        pass

    assert func_with_enum.priority == float_value
    assert func_with_float.priority == float_value
    assert func_with_enum.priority == func_with_float.priority


def test_execution_order_with_enum_priorities() -> None:
    """Test that execution order is correct when using enum priorities."""
    execution_order: list[str] = []

    @coroutine_with_priority(CoroPriority.PLATFORM)
    async def platform_func() -> None:
        execution_order.append("platform")

    @coroutine_with_priority(CoroPriority.CORE)
    async def core_func() -> None:
        execution_order.append("core")

    @coroutine_with_priority(CoroPriority.FINAL)
    async def final_func() -> None:
        execution_order.append("final")

    # Create event loop and add jobs
    loop = FakeEventLoop()
    loop.add_job(platform_func)
    loop.add_job(core_func)
    loop.add_job(final_func)

    # Run all tasks
    loop.flush_tasks()

    # Check execution order (higher priority runs first)
    assert execution_order == ["platform", "core", "final"]


def test_mixed_float_and_enum_priorities() -> None:
    """Test that mixing float and enum priorities works correctly."""
    execution_order: list[str] = []

    @coroutine_with_priority(1000.0)  # Same as PLATFORM
    async def func1() -> None:
        execution_order.append("func1")

    @coroutine_with_priority(CoroPriority.CORE)
    async def func2() -> None:
        execution_order.append("func2")

    @coroutine_with_priority(-1000.0)  # Same as FINAL
    async def func3() -> None:
        execution_order.append("func3")

    # Create event loop and add jobs
    loop = FakeEventLoop()
    loop.add_job(func2)
    loop.add_job(func3)
    loop.add_job(func1)

    # Run all tasks
    loop.flush_tasks()

    # Check execution order
    assert execution_order == ["func1", "func2", "func3"]


def test_enum_priority_comparison() -> None:
    """Test that enum priorities can be compared directly."""
    assert CoroPriority.PLATFORM > CoroPriority.NETWORK
    assert CoroPriority.NETWORK > CoroPriority.NETWORK_TRANSPORT
    assert CoroPriority.NETWORK_TRANSPORT > CoroPriority.CORE
    assert CoroPriority.CORE > CoroPriority.DIAGNOSTICS
    assert CoroPriority.DIAGNOSTICS > CoroPriority.STATUS
    assert CoroPriority.STATUS > CoroPriority.COMMUNICATION
    assert CoroPriority.COMMUNICATION > CoroPriority.APPLICATION
    assert CoroPriority.APPLICATION > CoroPriority.WEB
    assert CoroPriority.WEB > CoroPriority.AUTOMATION
    assert CoroPriority.AUTOMATION > CoroPriority.BUS
    assert CoroPriority.BUS > CoroPriority.COMPONENT
    assert CoroPriority.COMPONENT > CoroPriority.LATE
    assert CoroPriority.LATE > CoroPriority.WORKAROUNDS
    assert CoroPriority.WORKAROUNDS > CoroPriority.FINAL


def test_custom_priority_between_enum_values() -> None:
    """Test that custom float priorities between enum values work correctly."""
    execution_order: list[str] = []

    @coroutine_with_priority(CoroPriority.CORE)  # 100
    async def core_func() -> None:
        execution_order.append("core")

    @coroutine_with_priority(95.0)  # Between CORE and DIAGNOSTICS
    async def custom_func() -> None:
        execution_order.append("custom")

    @coroutine_with_priority(CoroPriority.DIAGNOSTICS)  # 90
    async def diag_func() -> None:
        execution_order.append("diagnostics")

    # Create event loop and add jobs
    loop = FakeEventLoop()
    loop.add_job(diag_func)
    loop.add_job(core_func)
    loop.add_job(custom_func)

    # Run all tasks
    loop.flush_tasks()

    # Check execution order
    assert execution_order == ["core", "custom", "diagnostics"]

"""Integration test for light automation triggers.

Tests that on_turn_on, on_turn_off, and on_state triggers work correctly
with the listener interface pattern.
"""

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_automations(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light on_turn_on, on_turn_off, and on_state triggers."""
    loop = asyncio.get_running_loop()

    # Futures for log line detection
    on_turn_on_future: asyncio.Future[bool] = loop.create_future()
    on_turn_off_future: asyncio.Future[bool] = loop.create_future()
    on_state_count = 0
    counting_enabled = False
    on_state_futures: list[asyncio.Future[bool]] = []

    def create_on_state_future() -> asyncio.Future[bool]:
        """Create a new future for on_state trigger."""
        future: asyncio.Future[bool] = loop.create_future()
        on_state_futures.append(future)
        return future

    def check_output(line: str) -> None:
        """Check log output for trigger messages."""
        nonlocal on_state_count
        if "TRIGGER: on_turn_on fired" in line:
            if not on_turn_on_future.done():
                on_turn_on_future.set_result(True)
        elif "TRIGGER: on_turn_off fired" in line:
            if not on_turn_off_future.done():
                on_turn_off_future.set_result(True)
        elif "TRIGGER: on_state fired" in line:
            # Only count on_state after we start testing
            if counting_enabled:
                on_state_count += 1
            # Complete any pending on_state futures
            for future in on_state_futures:
                if not future.done():
                    future.set_result(True)
                    break

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get entities
        entities = await client.list_entities_services()
        light = next(e for e in entities[0] if e.object_id == "test_light")

        # Start counting on_state events now
        counting_enabled = True

        # Test 1: Turn light on - should trigger on_turn_on and on_state
        on_state_future_1 = create_on_state_future()
        client.light_command(key=light.key, state=True)

        # Wait for on_turn_on trigger
        try:
            await asyncio.wait_for(on_turn_on_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("on_turn_on trigger did not fire")

        # Wait for on_state trigger
        try:
            await asyncio.wait_for(on_state_future_1, timeout=5.0)
        except TimeoutError:
            pytest.fail("on_state trigger did not fire after turn on")

        # Test 2: Turn light off - should trigger on_turn_off and on_state
        on_state_future_2 = create_on_state_future()
        client.light_command(key=light.key, state=False)

        # Wait for on_turn_off trigger
        try:
            await asyncio.wait_for(on_turn_off_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("on_turn_off trigger did not fire")

        # Wait for on_state trigger
        try:
            await asyncio.wait_for(on_state_future_2, timeout=5.0)
        except TimeoutError:
            pytest.fail("on_state trigger did not fire after turn off")

        # Verify on_state fired exactly twice (once for on, once for off)
        assert on_state_count == 2, (
            f"on_state should have triggered exactly twice, got {on_state_count}"
        )

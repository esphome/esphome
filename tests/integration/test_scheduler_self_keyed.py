"""Test the self-keyed scheduler API.

Verifies that `Scheduler::set_timeout(const void *, ...)` /
`set_interval(const void *, ...)` and the matching `cancel_*(const void *)`
overloads behave correctly: callbacks fire, distinct keys don't collide,
self-keyed and component-keyed namespaces are independent, and re-registering
the same key replaces the existing timer.
"""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_self_keyed(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test self-keyed scheduler API."""
    self_a_fired = asyncio.Event()
    self_b_error = asyncio.Event()
    self_c_fired = asyncio.Event()
    self_d_fired = asyncio.Event()
    self_shared_fired = asyncio.Event()
    component_7777_fired = asyncio.Event()
    self_interval_done = asyncio.Event()
    self_f_first_error = asyncio.Event()
    self_f_replacement_fired = asyncio.Event()
    all_tests_complete = asyncio.Event()

    def on_log_line(line: str) -> None:
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        if "Self timeout A fired" in clean_line:
            self_a_fired.set()
        elif "ERROR: Self timeout B" in clean_line:
            self_b_error.set()
        elif "Self timeout C fired" in clean_line:
            self_c_fired.set()
        elif "Self timeout D fired" in clean_line:
            self_d_fired.set()
        elif "Self timeout shared fired" in clean_line:
            self_shared_fired.set()
        elif "Component timeout 7777 fired" in clean_line:
            component_7777_fired.set()
        elif "Self interval E fired twice" in clean_line:
            self_interval_done.set()
        elif "ERROR: Self timeout F first registration" in clean_line:
            self_f_first_error.set()
        elif "Self timeout F replacement fired" in clean_line:
            self_f_replacement_fired.set()
        elif "All self-keyed tests complete" in clean_line:
            all_tests_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-self-keyed-test"

        try:
            await asyncio.wait_for(all_tests_complete.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("Not all self-keyed tests completed within 5 seconds")

        # Test 1: self-keyed timeout fires
        assert self_a_fired.is_set(), "Self timeout A should have fired"

        # Test 2: cancel_timeout(self) actually cancels
        assert not self_b_error.is_set(), "Self timeout B should have been cancelled"

        # Test 3: distinct self keys don't collide
        assert self_c_fired.is_set(), "Self timeout C should have fired"
        assert self_d_fired.is_set(), "Self timeout D should have fired"

        # Test 4: self-keyed and component-keyed namespaces are independent
        assert self_shared_fired.is_set(), "Self timeout shared should have fired"
        assert component_7777_fired.is_set(), "Component timeout 7777 should have fired"

        # Test 5: self-keyed interval fires repeatedly and cancels cleanly
        assert self_interval_done.is_set(), "Self interval E should have fired twice"

        # Test 6: re-registering same self-key replaces the previous timer
        assert not self_f_first_error.is_set(), (
            "Self timeout F first registration should have been replaced"
        )
        assert self_f_replacement_fired.is_set(), (
            "Self timeout F replacement should have fired"
        )

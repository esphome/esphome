"""Integration test for loop disable/enable functionality."""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_loop_disable_enable(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that components can disable and enable their loop() method."""
    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Track log messages and events
    log_messages: list[str] = []

    # Event fired when self_disable_10 component disables itself after 10 loops
    self_disable_10_disabled = asyncio.Event()
    # Event fired when normal_component reaches 10 loops
    normal_component_10_loops = asyncio.Event()
    # Event fired when redundant_enable component tests enabling when already enabled
    redundant_enable_tested = asyncio.Event()
    # Event fired when redundant_disable component tests disabling when already disabled
    redundant_disable_tested = asyncio.Event()
    # Event fired when self_disable_10 component is re-enabled and runs again (count > 10)
    self_disable_10_re_enabled = asyncio.Event()
    # Events for ISR component testing
    isr_component_disabled = asyncio.Event()
    isr_component_re_enabled = asyncio.Event()
    isr_component_pure_re_enabled = asyncio.Event()
    # Events for update component testing
    update_component_loop_disabled = asyncio.Event()
    update_component_manual_update_called = asyncio.Event()

    # Track loop counts for components
    self_disable_10_counts: list[int] = []
    normal_component_counts: list[int] = []
    isr_component_counts: list[int] = []
    # Track update component behavior
    update_component_loop_count = 0
    update_component_update_count = 0
    update_component_manual_update_count = 0

    def on_log_line(line: str) -> None:
        """Process each log line from the process output."""
        # Strip ANSI color codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        if (
            "loop_test_component" not in clean_line
            and "loop_test_isr_component" not in clean_line
            and "Manually calling component.update" not in clean_line
        ):
            return

        log_messages.append(clean_line)

        # Track specific events using the cleaned line
        if "[self_disable_10]" in clean_line:
            if "Loop count:" in clean_line:
                # Extract loop count
                try:
                    count = int(clean_line.split("Loop count: ")[1])
                    self_disable_10_counts.append(count)
                    # Check if component was re-enabled (count > 10)
                    if count > 10:
                        self_disable_10_re_enabled.set()
                except (IndexError, ValueError):
                    pass
            elif "Disabling self after 10 loops" in clean_line:
                self_disable_10_disabled.set()

        elif "[normal_component]" in clean_line and "Loop count:" in clean_line:
            try:
                count = int(clean_line.split("Loop count: ")[1])
                normal_component_counts.append(count)
                if count >= 10:
                    normal_component_10_loops.set()
            except (IndexError, ValueError):
                pass

        elif (
            "[redundant_enable]" in clean_line
            and "Testing enable when already enabled" in clean_line
        ):
            redundant_enable_tested.set()

        elif (
            "[redundant_disable]" in clean_line
            and "Testing disable when will be disabled" in clean_line
        ):
            redundant_disable_tested.set()

        # ISR component events
        elif "[isr_test]" in clean_line:
            if "ISR component loop count:" in clean_line:
                count = int(clean_line.split("ISR component loop count: ")[1])
                isr_component_counts.append(count)
            elif "Disabling after 5 loops" in clean_line:
                isr_component_disabled.set()
            elif "Running after ISR re-enable!" in clean_line:
                isr_component_re_enabled.set()
            elif "Running after pure ISR re-enable!" in clean_line:
                isr_component_pure_re_enabled.set()

        # Update component events
        elif "[update_test]" in clean_line:
            if "LoopTestUpdateComponent loop count:" in clean_line:
                nonlocal update_component_loop_count
                update_component_loop_count = int(
                    clean_line.split("LoopTestUpdateComponent loop count: ")[1]
                )
            elif "LoopTestUpdateComponent update() called" in clean_line:
                nonlocal update_component_update_count
                update_component_update_count += 1
                if "Manually calling component.update" in " ".join(log_messages[-5:]):
                    nonlocal update_component_manual_update_count
                    update_component_manual_update_count += 1
                    update_component_manual_update_called.set()
            elif "Disabling loop after" in clean_line:
                update_component_loop_disabled.set()

    # Write, compile and run the ESPHome device with log callback
    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect and get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "loop-test"

        # Wait for self_disable_10 to disable itself
        try:
            await asyncio.wait_for(self_disable_10_disabled.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail("self_disable_10 did not disable itself within 10 seconds")

        # Verify it ran at least 10 times before disabling
        assert len([c for c in self_disable_10_counts if c <= 10]) == 10, (
            f"Expected exactly 10 loops before disable, got {[c for c in self_disable_10_counts if c <= 10]}"
        )
        assert self_disable_10_counts[:10] == list(range(1, 11)), (
            f"Expected first 10 counts to be 1-10, got {self_disable_10_counts[:10]}"
        )

        # Wait for normal_component to run at least 10 times
        try:
            await asyncio.wait_for(normal_component_10_loops.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"normal_component did not reach 10 loops within timeout, got {len(normal_component_counts)}"
            )

        # Wait for redundant operation tests
        try:
            await asyncio.wait_for(redundant_enable_tested.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail("redundant_enable did not test enabling when already enabled")

        try:
            await asyncio.wait_for(redundant_disable_tested.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail(
                "redundant_disable did not test disabling when will be disabled"
            )

        # Wait to see if self_disable_10 gets re-enabled
        try:
            await asyncio.wait_for(self_disable_10_re_enabled.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("self_disable_10 was not re-enabled within 5 seconds")

        # Component was re-enabled - verify it ran more times
        later_self_disable_counts = [c for c in self_disable_10_counts if c > 10]
        assert later_self_disable_counts, (
            "self_disable_10 was re-enabled but did not run additional times"
        )

        # Test ISR component functionality
        # Wait for ISR component to disable itself after 5 loops
        try:
            await asyncio.wait_for(isr_component_disabled.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail("ISR component did not disable itself within 3 seconds")

        # Verify it ran exactly 5 times before disabling
        first_run_counts = [c for c in isr_component_counts if c <= 5]
        assert len(first_run_counts) == 5, (
            f"Expected 5 loops before disable, got {first_run_counts}"
        )

        # Wait for component to be re-enabled by periodic ISR simulation and run again
        try:
            await asyncio.wait_for(isr_component_re_enabled.wait(), timeout=2.0)
        except TimeoutError:
            pytest.fail("ISR component was not re-enabled after ISR call")

        # Verify it's running again after ISR enable
        count_after_isr = len(isr_component_counts)
        assert count_after_isr > 5, (
            f"Component didn't run after ISR enable: got {count_after_isr} counts total"
        )

        # Wait for pure ISR enable (no main loop enable) to work
        try:
            await asyncio.wait_for(isr_component_pure_re_enabled.wait(), timeout=2.0)
        except TimeoutError:
            pytest.fail("ISR component was not re-enabled by pure ISR call")

        # Verify it ran after pure ISR enable
        final_count = len(isr_component_counts)
        assert final_count > 10, (
            f"Component didn't run after pure ISR enable: got {final_count} counts total"
        )

        # Test component.update functionality when loop is disabled
        # Wait for update component to disable its loop
        try:
            await asyncio.wait_for(update_component_loop_disabled.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail("Update component did not disable its loop within 3 seconds")

        # Verify it ran exactly 3 loops before disabling
        assert update_component_loop_count == 3, (
            f"Expected 3 loop iterations before disable, got {update_component_loop_count}"
        )

        # Wait for manual component.update to be called
        try:
            await asyncio.wait_for(
                update_component_manual_update_called.wait(), timeout=5.0
            )
        except TimeoutError:
            pytest.fail("Manual component.update was not called within 5 seconds")

        # The key test: verify that manual component.update worked after loop was disabled
        assert update_component_manual_update_count >= 1, (
            "component.update did not fire after loop was disabled"
        )

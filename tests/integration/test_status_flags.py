"""Integration tests for Component::status_set/clear_warning/error propagation.

Verifies that toggling STATUS_LED_WARNING / STATUS_LED_ERROR on individual
components correctly updates the app-wide bits on Application::app_state_,
AND that the status_led_light component actually responds to those bits
by writing to its output (the full chain from component.status_set_warning
→ App.app_state_ → status_led_light.loop() reading get_app_state()).

Exercises the multi-component OR semantics (the app bit stays set while
any component still has the flag, and only clears when the last component
clears its bit), the independence of warning and error, and the actual
status_led_light read of the bits via a fake template output that counts
writes.
"""

from __future__ import annotations

import asyncio

import pytest

from .state_utils import InitialStateHelper, SensorTracker, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction

# Time to let the host-mode main loop run so status_led_light.loop() can
# execute enough iterations to produce measurable write-count changes on
# the fake template output. 300 ms is well above the minimum needed.
STATUS_LED_SETTLE_S = 0.3


@pytest.mark.asyncio
async def test_status_flags(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, services = await client.list_entities_services()

        # Map every custom API service by name for the test to execute.
        svc = {s.name: s for s in services}
        for name in (
            "set_warning_a",
            "clear_warning_a",
            "set_warning_b",
            "clear_warning_b",
            "set_error_a",
            "clear_error_a",
            "set_error_b",
            "clear_error_b",
            "snapshot_led",
        ):
            assert name in svc, f"service {name} not registered"

        # Track every sensor we care about. SensorTracker gives us
        # expect(value) / expect_any() futures that resolve when a
        # matching state arrives; much simpler than manual bookkeeping.
        tracker = SensorTracker(
            [
                "app_warning_bit",
                "app_error_bit",
                "status_led_writes",
                "status_led_last_state",
            ]
        )
        tracker.key_to_sensor.update(
            build_key_to_entity_mapping(entities, list(tracker.sensor_states.keys()))
        )

        # Swallow initial state broadcasts so the test only reacts to
        # state changes triggered by our service calls.
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(tracker.on_state))
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        async def call(name: str) -> None:
            await client.execute_service(svc[name], {})

        async def call_and_expect_bits(
            service_name: str, *, warning: float, error: float
        ) -> None:
            """Execute a service and wait for both app bit sensors to match.

            Each bit-toggling service calls component.update on both
            app_warning_bit and app_error_bit, so both sensors publish.
            """
            futures = tracker.expect_all(
                {"app_warning_bit": warning, "app_error_bit": error}
            )
            await call(service_name)
            await tracker.await_all(futures)

        async def snapshot_led_writes() -> int:
            """Trigger a publish of the fake status_led output counter and return it."""
            future = tracker.expect_any("status_led_writes")
            await call("snapshot_led")
            await tracker.await_change(future, "status_led_writes")
            return int(tracker.sensor_states["status_led_writes"][-1])

        # ---- Baseline: everything clean ----
        await call_and_expect_bits("clear_warning_a", warning=0.0, error=0.0)

        # ================================================================
        # Part 1 — STATUS_LED_WARNING propagation to App.app_state_
        # ================================================================

        # Single component set/clear
        await call_and_expect_bits("set_warning_a", warning=1.0, error=0.0)
        await call_and_expect_bits("clear_warning_a", warning=0.0, error=0.0)

        # Multi-component OR: both set, clear A, bit stays (B still has it), clear B, gone
        await call_and_expect_bits("set_warning_a", warning=1.0, error=0.0)
        await call_and_expect_bits("set_warning_b", warning=1.0, error=0.0)
        await call_and_expect_bits("clear_warning_a", warning=1.0, error=0.0)
        await call_and_expect_bits("clear_warning_b", warning=0.0, error=0.0)

        # Opposite clear order
        await call_and_expect_bits("set_warning_a", warning=1.0, error=0.0)
        await call_and_expect_bits("set_warning_b", warning=1.0, error=0.0)
        await call_and_expect_bits("clear_warning_b", warning=1.0, error=0.0)
        await call_and_expect_bits("clear_warning_a", warning=0.0, error=0.0)

        # ================================================================
        # Part 2 — STATUS_LED_ERROR propagation (same scenarios)
        # ================================================================

        await call_and_expect_bits("set_error_a", warning=0.0, error=1.0)
        await call_and_expect_bits("clear_error_a", warning=0.0, error=0.0)

        await call_and_expect_bits("set_error_a", warning=0.0, error=1.0)
        await call_and_expect_bits("set_error_b", warning=0.0, error=1.0)
        await call_and_expect_bits("clear_error_a", warning=0.0, error=1.0)
        await call_and_expect_bits("clear_error_b", warning=0.0, error=0.0)

        # ================================================================
        # Part 3 — warning and error are independent
        # ================================================================

        await call_and_expect_bits("set_warning_a", warning=1.0, error=0.0)
        await call_and_expect_bits("set_error_b", warning=1.0, error=1.0)
        await call_and_expect_bits("clear_warning_a", warning=0.0, error=1.0)
        await call_and_expect_bits("clear_error_b", warning=0.0, error=0.0)

        # ================================================================
        # Part 4 — status_led_light actually reads App.app_state_
        # ================================================================
        # The fake status_led_light output increments status_led_write_count
        # on every write. status_led_light::loop() writes its output on every
        # iteration while an error/warning bit is set, so after holding a
        # warning for ~300 ms we should see the counter move significantly.
        # This is the end-to-end proof that the bits we set above actually
        # reach status_led_light and drive its behavior.

        count_before_warning = await snapshot_led_writes()
        await call_and_expect_bits("set_warning_a", warning=1.0, error=0.0)
        # Let status_led_light's loop run long enough to toggle the pin
        # several times (it reads get_app_state() every main loop iteration).
        await asyncio.sleep(STATUS_LED_SETTLE_S)
        count_after_warning = await snapshot_led_writes()
        assert count_after_warning > count_before_warning, (
            "status_led_light did not respond to STATUS_LED_WARNING being set: "
            f"write count stayed at {count_before_warning} → {count_after_warning}. "
            "The full chain Component::status_set_warning → App.app_state_ → "
            "status_led_light::loop reading get_app_state() is broken."
        )
        await call_and_expect_bits("clear_warning_a", warning=0.0, error=0.0)

        # Same check for ERROR
        count_before_error = await snapshot_led_writes()
        await call_and_expect_bits("set_error_a", warning=0.0, error=1.0)
        await asyncio.sleep(STATUS_LED_SETTLE_S)
        count_after_error = await snapshot_led_writes()
        assert count_after_error > count_before_error, (
            "status_led_light did not respond to STATUS_LED_ERROR being set: "
            f"write count stayed at {count_before_error} → {count_after_error}. "
        )
        await call_and_expect_bits("clear_error_a", warning=0.0, error=0.0)

        # ---- Set → clear → re-set round-trip ----
        # After clearing, status_led_light stops writing (steady state).
        # Re-setting the flag must make it resume. This guards against a
        # future idle optimization (e.g. #15642) where status_led disables
        # its own loop when idle: if the re-enable path were broken, the
        # second set would not produce writes.
        #
        # Snapshot AFTER the clear to avoid counting writes that were still
        # in-flight from the error-set phase.
        count_after_clear = await snapshot_led_writes()
        await asyncio.sleep(STATUS_LED_SETTLE_S)
        count_after_idle = await snapshot_led_writes()
        assert count_after_idle - count_after_clear <= 5, (
            "status_led_light kept writing after warning/error was cleared: "
            f"count grew from {count_after_clear} to {count_after_idle}. "
            "Expected it to stop writing once all status bits were clear."
        )
        # Re-set warning — writes must resume.
        await call_and_expect_bits("set_warning_a", warning=1.0, error=0.0)
        await asyncio.sleep(STATUS_LED_SETTLE_S)
        count_after_reset = await snapshot_led_writes()
        assert count_after_reset > count_after_idle + 5, (
            "status_led_light did not resume writing after re-setting "
            f"STATUS_LED_WARNING: count went from {count_after_idle} to "
            f"{count_after_reset}. If an idle optimization disabled the "
            "loop, the re-enable path may be broken."
        )
        await call_and_expect_bits("clear_warning_a", warning=0.0, error=0.0)

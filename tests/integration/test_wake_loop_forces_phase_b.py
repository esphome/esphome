"""Test that wake_loop_threadsafe() forces a component-phase iteration.

Regression test for the wake-request flag added to Application::loop()'s
Phase A / Phase B gate. Background producers (MQTT RX, USB RX, BLE event,
etc.) call App.wake_loop_threadsafe() expecting their component's loop()
to drain queued work; if the component phase stays gated by loop_interval_,
the work waits up to loop_interval_ ms instead of running on the next tick.

Setup:
- App.set_loop_interval(2000) — a wide gate that would clearly mask the bug.
- A test component spawns a detached std::thread that sleeps 50 ms and then
  calls App.wake_loop_threadsafe() from a non-main thread.
- The on_boot block snapshots the component's loop counter before/after a
  500 ms observation window.

Without the fix, delta=0 (the gate holds Phase B for ~2 s).
With the fix, delta>=1 (the wake forces Phase B within one tick of the wake).
"""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_wake_loop_forces_phase_b(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A wake_loop_threadsafe() call from a background thread must trigger the
    component phase within the next tick, even when loop_interval_ is raised
    well above the observation window."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()
    result: asyncio.Future[tuple[int, int]] = loop.create_future()

    def on_log_line(line: str) -> None:
        match = re.search(r"WAKE_RESULT delta=(\d+) elapsed=(\d+)", line)
        if match and not result.done():
            result.set_result((int(match.group(1)), int(match.group(2))))

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "wake-loop-phase-b"

        try:
            delta, elapsed = await asyncio.wait_for(result, timeout=15.0)
        except TimeoutError:
            pytest.fail("WAKE_RESULT marker never appeared")

        # Without the fix, delta would be 0 — loop_interval_=2000ms held
        # Phase B off for the full 500ms observation window. With the fix
        # the wake from the background thread (~50ms after start) forces
        # Phase B on the next tick, so the counter increments at least once.
        assert delta >= 1, (
            f"wake_loop_threadsafe() from a background thread should force "
            f"Phase B within the next tick; observed delta={delta} after "
            f"{elapsed}ms with loop_interval_=2000ms"
        )

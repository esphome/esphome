"""Test that a fast scheduler item does not pull the component phase forward.

Regression test for the original ~128 Hz → ~62 Hz bug fixed by decoupling
Application::loop() component-phase cadence from scheduler wake timing.

Setup:
- loop_interval_ left at its default (16 ms → ~62 Hz component phase).
- Scheduler interval at 5 ms (well under the old loop_interval_/2 = 8 ms floor).

Before the decoupling fix the ``std::max(next_schedule, delay_time / 2)`` floor
clamped the sleep to ~8 ms whenever any scheduler item was due sooner than
loop_interval_/2. That pulled the component phase forward to ~128 Hz — twice
what the documented ~62 Hz default promised. After the fix the component
phase stays at ~62 Hz regardless of scheduler activity.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

MEASUREMENT_RE = re.compile(r"MEASUREMENT_DONE loop_delta=(\d+)")


@pytest.mark.asyncio
async def test_loop_interval_default_not_pulled_forward(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Fast scheduler item must not pull component phase past default ~62 Hz."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "loop-default-not-pulled")

        line = await wait_or_fail(
            waiter.wait_for("MEASUREMENT_DONE loop_delta=", timeout=20.0),
            10.0,
            "MEASUREMENT_DONE marker never appeared",
        )
        match = MEASUREMENT_RE.search(line)
        assert match, line
        loop_delta = int(match[1])

        # Observation window = 2s, loop_interval_ default = 16ms → ~62 Hz →
        # ~125 component-phase iterations expected.
        # Pre-fix behavior: the 5 ms scheduler interval tripped the old
        # delay_time/2 = 8 ms floor, pulling the phase to ~128 Hz → ~256.
        # Upper bound 180 is comfortably below the ~256 pre-fix rate but
        # above the ~125 nominal with CI jitter.
        # Lower bound 80 covers very slow CI hosts without permitting a
        # complete regression.
        assert 80 <= loop_delta <= 180, (
            f"Component loop at default loop_interval_ should fire ~125 times "
            f"in 2s (≈62 Hz × 2s); got {loop_delta}. Values >200 indicate the "
            f"scheduler is again pulling the component phase forward."
        )

"""Test that loop_interval_ no longer clamps scheduler cadence.

Regression test for the decoupling of Application::loop() component-phase
cadence from scheduler wake timing.

Setup:
- App.set_loop_interval(500) — raised for power-savings style cadence
- Scheduler interval at 50ms — should fire at 50ms regardless of loop_interval_
- Component loop (LoopTestComponent) — should run at 500ms cadence

Before the decoupling fix the old `std::max(next_schedule, delay_time / 2)`
floor clamped the sleep to ~250ms, so the 50ms scheduler only fired ~8 times
per 2s (vs the ~40 expected). After the fix the scheduler fires close to its
requested cadence while the component phase stays gated at loop_interval_.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

MEASUREMENT_RE = re.compile(r"MEASUREMENT_DONE loop_delta=(\d+) sched_delta=(\d+)")


@pytest.mark.asyncio
async def test_loop_interval_decoupling(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Raised loop_interval_ must not clamp scheduler item cadence."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "loop-interval-decouple")

        line = await wait_or_fail(
            waiter.wait_for("MEASUREMENT_DONE loop_delta=", timeout=20.0),
            10.0,
            "MEASUREMENT_DONE marker never appeared",
        )
        match = MEASUREMENT_RE.search(line)
        assert match, line
        loop_delta, sched_delta = int(match[1]), int(match[2])

        # Observation window = 2s, loop_interval_ = 500ms.
        # Component phase should fire ~4 times in 2s. The upper bound must be
        # less than 8: the pre-decoupling behavior clamped to ~250ms cadence
        # giving ~8 loops/2s, so allowing 8 would let the old behavior pass.
        # Lower bound 3 (not 2) keeps the test honest: a >30% slowdown from
        # the ~4 nominal is not normal CI jitter and should fail.
        assert 3 <= loop_delta <= 6, (
            f"Component loop should fire ~4 times in 2s at loop_interval=500ms, "
            f"got {loop_delta}"
        )

        # Scheduler interval = 50ms → ~40 fires in 2s. Before the decoupling
        # fix this clamped to ~8 fires. Assert >= 20 to catch the old clamped
        # behavior with comfortable jitter headroom for slow CI hosts.
        assert sched_delta >= 20, (
            f"50ms scheduler interval should fire ~40 times in 2s but only "
            f"fired {sched_delta}. This indicates loop_interval_ is still "
            f"clamping scheduler cadence."
        )

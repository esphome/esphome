"""A callback capture's destructor may call the scheduler on every recycle path.

The scheduler frees a recycled item's callable after releasing its lock, so on
host a destructor that schedules a timer does not deadlock. debug_scheduler is
on, so the leak check also runs while items wait to be freed.
"""

from __future__ import annotations

import pytest

from .conftest import run_binary
from .log_utils import LineWaiter
from .types import CompileFunction, ConfigWriter


@pytest.mark.asyncio
async def test_scheduler_capture_destructor(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
) -> None:
    """Every rescheduled timer runs and the device keeps running."""
    waiter = LineWaiter()
    binary_path = await compile_esphome(await write_yaml_config(yaml_config))

    async with run_binary(binary_path, line_callback=waiter.callback) as (proc, _):
        await waiter.wait_for_each(
            "DONT_RUN_RESCHEDULED",
            "CANCELLED_RESCHEDULED",
            "AFTER_RUN_RESCHEDULED",
            "STILL_RUNNING",
            timeout=5.0,
        )
        assert proc.returncode is None
        assert not any("LEAK DETECTED" in line for line in waiter.lines)

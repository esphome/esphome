"""A timeout scheduled by a component loop must affect the current sleep."""

from __future__ import annotations

import re

import pytest

from .conftest import run_binary
from .log_utils import LineWaiter
from .types import CompileFunction, ConfigWriter
from .wait_utils import wait_or_fail

RESULT_RE = re.compile(r"SCHEDULER_LOOP_PHASE_RESULT elapsed=(\d+)")


@pytest.mark.asyncio
async def test_scheduler_loop_phase_timeout(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
) -> None:
    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    waiter = LineWaiter()

    async with run_binary(binary_path, line_callback=waiter.callback):
        line = await wait_or_fail(
            waiter.wait_for("SCHEDULER_LOOP_PHASE_RESULT elapsed=", timeout=30.0),
            15.0,
            "loop-phase timeout did not fire within 15 seconds",
        )
        match = RESULT_RE.search(line)
        assert match, line
        elapsed = int(match[1])

        assert elapsed < 1000, (
            f"loop-phase timeout should interrupt the five-second sleep, but fired after {elapsed}ms"
        )

"""A timeout scheduled by a component loop must affect the current sleep."""

from __future__ import annotations

import asyncio
import re

import pytest

from .conftest import run_binary
from .types import CompileFunction, ConfigWriter


@pytest.mark.asyncio
async def test_scheduler_loop_phase_timeout(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
) -> None:
    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    loop = asyncio.get_running_loop()
    timeout_result: asyncio.Future[int] = loop.create_future()

    def on_log_line(line: str) -> None:
        match = re.search(r"SCHEDULER_LOOP_PHASE_RESULT elapsed=(\d+)", line)
        if match and not timeout_result.done():
            timeout_result.set_result(int(match.group(1)))

    async with run_binary(binary_path, line_callback=on_log_line):
        try:
            elapsed = await asyncio.wait_for(timeout_result, timeout=15.0)
        except TimeoutError:
            pytest.fail("loop-phase timeout did not fire within 15 seconds")

        assert elapsed < 1000, (
            f"loop-phase timeout should interrupt the five-second sleep, but fired after {elapsed}ms"
        )

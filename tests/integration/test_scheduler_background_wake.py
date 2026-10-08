"""A background scheduler insertion must interrupt the loop's current sleep."""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_background_wake(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()
    result: asyncio.Future[int] = loop.create_future()

    def on_log_line(line: str) -> None:
        match = re.search(r"SCHEDULER_WAKE_RESULT elapsed=(\d+)", line)
        if match and not result.done():
            result.set_result(int(match.group(1)))

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-background-wake"

        try:
            elapsed = await asyncio.wait_for(result, timeout=10.0)
        except TimeoutError:
            pytest.fail("background scheduler timeout did not fire")

        assert elapsed < 1000, (
            f"background timeout should interrupt the five-second sleep; "
            f"it fired after {elapsed}ms"
        )

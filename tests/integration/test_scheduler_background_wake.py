"""A background scheduler insertion must interrupt the loop's current sleep.

Covers both set_timeout and defer: a zero-delay insert from another thread
takes the separate defer queue on multi-threaded builds.
"""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import require_entity
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
    results: dict[int, asyncio.Future[tuple[int, int]]] = {
        100: loop.create_future(),
        0: loop.create_future(),
    }

    def on_log_line(line: str) -> None:
        match = re.search(
            r"SCHEDULER_WAKE_RESULT delay=(\d+) elapsed=(\d+) loop_delta=(-?\d+)",
            line,
        )
        if match is None:
            return
        result = results[int(match.group(1))]
        if not result.done():
            result.set_result((int(match.group(2)), int(match.group(3))))

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-background-wake"
        entities, _ = await client.list_entities_services()
        for object_id, delay in (
            ("start_scheduler_timeout", 100),
            ("start_scheduler_defer", 0),
        ):
            start_button = require_entity(
                entities,
                object_id,
                ButtonInfo,
                description=f"{object_id} button",
            )
            client.button_command(start_button.key)

            try:
                elapsed, loop_delta = await asyncio.wait_for(
                    results[delay], timeout=10.0
                )
            except TimeoutError:
                pytest.fail(f"background insert with delay={delay} did not fire")

            assert elapsed < 1000, (
                f"background insert with delay={delay} should interrupt the "
                f"five-second sleep; it fired after {elapsed}ms"
            )
            assert loop_delta == 0, (
                f"scheduler-only wake must not run component loops; observed "
                f"loop_delta={loop_delta} for delay={delay}"
            )

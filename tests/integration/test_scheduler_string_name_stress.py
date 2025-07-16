"""Stress test for heap scheduler with std::string names from multiple threads."""

import asyncio
from pathlib import Path
import re

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_string_name_stress(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that set_timeout/set_interval with std::string names doesn't crash when called from multiple threads."""

    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Create a future to signal test completion
    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[None] = loop.create_future()

    # Track executed callbacks and any crashes
    executed_callbacks: set[int] = set()
    error_messages: list[str] = []

    def on_log_line(line: str) -> None:
        # Check for crash indicators
        if any(
            indicator in line.lower()
            for indicator in [
                "segfault",
                "abort",
                "assertion",
                "heap corruption",
                "use after free",
            ]
        ):
            error_messages.append(line)
            if not test_complete_future.done():
                test_complete_future.set_exception(Exception(f"Crash detected: {line}"))
            return

        # Track executed callbacks
        match = re.search(r"Executed string-named callback (\d+)", line)
        if match:
            callback_id = int(match.group(1))
            executed_callbacks.add(callback_id)

        # Check for completion
        if (
            "String name stress test complete" in line
            and not test_complete_future.done()
        ):
            test_complete_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "sched-string-name-stress"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_stress_test_service: UserService | None = None
        for service in services:
            if service.name == "run_string_name_stress_test":
                run_stress_test_service = service
                break

        assert run_stress_test_service is not None, (
            "run_string_name_stress_test service not found"
        )

        # Call the service to start the test
        client.execute_service(run_stress_test_service, {})

        # Wait for test to complete or crash
        try:
            await asyncio.wait_for(test_complete_future, timeout=30.0)
        except TimeoutError:
            pytest.fail(
                f"String name stress test timed out. Executed {len(executed_callbacks)} callbacks. "
                f"This might indicate a deadlock."
            )

        # Verify no errors occurred (crashes already handled by exception)
        assert not error_messages, f"Errors detected during test: {error_messages}"

        # Verify we executed all 1000 callbacks (10 threads × 100 callbacks each)
        assert len(executed_callbacks) == 1000, (
            f"Expected 1000 callbacks but got {len(executed_callbacks)}"
        )

        # Verify each callback ID was executed exactly once
        for i in range(1000):
            assert i in executed_callbacks, f"Callback {i} was not executed"

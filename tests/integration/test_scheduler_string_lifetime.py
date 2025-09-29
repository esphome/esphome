"""String lifetime test - verify scheduler handles string destruction correctly."""

import asyncio
from pathlib import Path
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_string_lifetime(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler correctly handles string lifetimes when strings go out of scope."""

    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Create events for synchronization
    test1_complete = asyncio.Event()
    test2_complete = asyncio.Event()
    test3_complete = asyncio.Event()
    test4_complete = asyncio.Event()
    test5_complete = asyncio.Event()
    all_tests_complete = asyncio.Event()

    # Track test progress
    test_stats = {
        "tests_passed": 0,
        "tests_failed": 0,
        "errors": [],
        "current_test": None,
        "test_callbacks_executed": {},
    }

    def on_log_line(line: str) -> None:
        # Track test-specific events
        if "Test 1 complete" in line:
            test1_complete.set()
        elif "Test 2 complete" in line:
            test2_complete.set()
        elif "Test 3 complete" in line:
            test3_complete.set()
        elif "Test 4 complete" in line:
            test4_complete.set()
        elif "Test 5 complete" in line:
            test5_complete.set()

        # Track individual callback executions
        callback_match = re.search(r"Callback '(.+?)' executed", line)
        if callback_match:
            callback_name = callback_match.group(1)
            test_stats["test_callbacks_executed"][callback_name] = True

        # Track test results from the C++ test output
        if "Tests passed:" in line and "string_lifetime" in line:
            # Extract the number from "Tests passed: 32"
            match = re.search(r"Tests passed:\s*(\d+)", line)
            if match:
                test_stats["tests_passed"] = int(match.group(1))
        elif "Tests failed:" in line and "string_lifetime" in line:
            match = re.search(r"Tests failed:\s*(\d+)", line)
            if match:
                test_stats["tests_failed"] = int(match.group(1))
        elif "ERROR" in line and "string_lifetime" in line:
            test_stats["errors"].append(line)

        # Check for memory corruption indicators
        if any(
            indicator in line.lower()
            for indicator in [
                "use after free",
                "heap corruption",
                "segfault",
                "abort",
                "assertion",
                "sanitizer",
                "bad memory",
                "invalid pointer",
            ]
        ):
            pytest.fail(f"Memory corruption detected: {line}")

        # Check for completion
        if "String lifetime tests complete" in line:
            all_tests_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-string-lifetime-test"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test services
        test_services = {}
        for service in services:
            if service.name == "run_test1":
                test_services["test1"] = service
            elif service.name == "run_test2":
                test_services["test2"] = service
            elif service.name == "run_test3":
                test_services["test3"] = service
            elif service.name == "run_test4":
                test_services["test4"] = service
            elif service.name == "run_test5":
                test_services["test5"] = service
            elif service.name == "run_final_check":
                test_services["final"] = service

        # Ensure all services are found
        required_services = ["test1", "test2", "test3", "test4", "test5", "final"]
        for service_name in required_services:
            assert service_name in test_services, f"{service_name} service not found"

        # Run tests sequentially, waiting for each to complete
        try:
            # Test 1
            client.execute_service(test_services["test1"], {})
            await asyncio.wait_for(test1_complete.wait(), timeout=5.0)

            # Test 2
            client.execute_service(test_services["test2"], {})
            await asyncio.wait_for(test2_complete.wait(), timeout=5.0)

            # Test 3
            client.execute_service(test_services["test3"], {})
            await asyncio.wait_for(test3_complete.wait(), timeout=5.0)

            # Test 4
            client.execute_service(test_services["test4"], {})
            await asyncio.wait_for(test4_complete.wait(), timeout=5.0)

            # Test 5
            client.execute_service(test_services["test5"], {})
            await asyncio.wait_for(test5_complete.wait(), timeout=5.0)

            # Final check
            client.execute_service(test_services["final"], {})
            await asyncio.wait_for(all_tests_complete.wait(), timeout=5.0)

        except TimeoutError:
            pytest.fail(f"String lifetime test timed out. Stats: {test_stats}")

        # Check for any errors
        assert test_stats["tests_failed"] == 0, f"Tests failed: {test_stats['errors']}"

        # Verify we had the expected number of passing tests
        assert test_stats["tests_passed"] == 30, (
            f"Expected exactly 30 tests to pass, but got {test_stats['tests_passed']}"
        )

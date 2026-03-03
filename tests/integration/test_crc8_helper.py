"""Integration test for CRC8 helper function."""

from __future__ import annotations

import asyncio
from pathlib import Path

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_crc8_helper(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the CRC8 helper function through integration testing."""
    # Get the path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Track test completion with asyncio.Event
    test_complete = asyncio.Event()

    # Track test results
    test_results = {
        "dallas_maxim": False,
        "sensirion": False,
        "pec": False,
        "parameter_equivalence": False,
        "edge_cases": False,
        "component_compatibility": False,
        "setup_started": False,
    }

    def on_log_line(line):
        """Process log lines to track test progress and results."""
        # Track test start
        if "CRC8 Helper Function Integration Test Starting" in line:
            test_results["setup_started"] = True

        # Track test completion
        elif "CRC8 Integration Test Complete" in line:
            test_complete.set()

        # Track individual test results
        elif "ALL TESTS PASSED" in line:
            if "Dallas/Maxim CRC8" in line:
                test_results["dallas_maxim"] = True
            elif "Sensirion CRC8" in line:
                test_results["sensirion"] = True
            elif "PEC CRC8" in line:
                test_results["pec"] = True
            elif "Parameter equivalence" in line:
                test_results["parameter_equivalence"] = True
            elif "Edge cases" in line:
                test_results["edge_cases"] = True
            elif "Component compatibility" in line:
                test_results["component_compatibility"] = True

        # Log failures for debugging
        elif "TEST FAILED:" in line or "SUBTEST FAILED:" in line:
            print(f"CRC8 Test Failure: {line}")

    # Compile and run the test
    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "crc8-helper-test"

        # Wait for tests to complete with timeout
        try:
            await asyncio.wait_for(test_complete.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("CRC8 integration test timed out after 5 seconds")

        # Verify all tests passed
        assert test_results["setup_started"], "CRC8 test setup never started"
        assert test_results["dallas_maxim"], "Dallas/Maxim CRC8 test failed"
        assert test_results["sensirion"], "Sensirion CRC8 test failed"
        assert test_results["pec"], "PEC CRC8 test failed"
        assert test_results["parameter_equivalence"], (
            "Parameter equivalence test failed"
        )
        assert test_results["edge_cases"], "Edge cases test failed"
        assert test_results["component_compatibility"], (
            "Component compatibility test failed"
        )

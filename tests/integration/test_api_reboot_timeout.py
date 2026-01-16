"""Test API server reboot timeout functionality."""

import asyncio
import re

import pytest

from .types import RunCompiledFunction


@pytest.mark.asyncio
async def test_api_reboot_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Test that the device reboots when no API clients connect within the timeout."""
    loop = asyncio.get_running_loop()
    reboot_future = loop.create_future()
    reboot_pattern = re.compile(r"No clients; rebooting")

    def check_output(line: str) -> None:
        """Check output for reboot message."""
        if not reboot_future.done() and reboot_pattern.search(line):
            reboot_future.set_result(True)

    # Run the device without connecting any API client
    async with run_compiled(yaml_config, line_callback=check_output):
        # Wait for reboot with timeout
        # (0.5s reboot timeout + some margin for processing)
        try:
            await asyncio.wait_for(reboot_future, timeout=2.0)
        except TimeoutError:
            pytest.fail("Device did not reboot within expected timeout")

    # Test passes if we get here - reboot was detected

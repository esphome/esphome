"""Test API server reboot timeout functionality."""

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_api_reboot_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Test that the device reboots when no API clients connect within the timeout."""
    waiter = LineWaiter()

    # Run the device without connecting any API client
    async with run_compiled(yaml_config, line_callback=waiter.callback):
        # 0.5s reboot timeout + some margin for processing
        await wait_or_fail(
            waiter.wait_for("No clients; rebooting"),
            2.0,
            "Device did not reboot within expected timeout",
        )

    # Test passes if we get here - reboot was detected

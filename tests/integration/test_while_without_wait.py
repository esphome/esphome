"""A while loop whose body never waits must still let the main loop run."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_while_without_wait(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The loop ends once the main loop turns the condition off, without a crash or hang."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()

        await client.execute_service(require_service(services, "run_lambda_body"), {})
        await waiter.wait_count(
            "LAMBDA_BODY done", 1, 5.0, "lambda body loop did not end"
        )

        await client.execute_service(
            require_service(services, "run_script_body"), {"tag": 7}
        )
        await waiter.wait_count(
            "SCRIPT_BODY done tag=7", 1, 5.0, "script body loop did not end"
        )

        # Still answering after both loops
        await client.device_info()

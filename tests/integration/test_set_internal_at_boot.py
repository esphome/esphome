"""Integration test for set_internal() called during and after setup."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_set_internal_at_boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """set_internal() in on_boot changes API exposure, later calls log an error."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, services = await client.list_entities_services()
        names = {entity.name for entity in entities}

        assert "Hidden At Boot" not in names
        assert "Shown At Boot" in names
        assert "Untouched" in names

        late = next(s for s in services if s.name == "set_internal_late")
        await client.execute_service(late, {})
        await waiter.wait_for(
            "'Untouched'",
            "set_internal() after setup is undefined behavior",
            timeout=5.0,
        )

        # Still written during the deprecation window, ignored from 2027.3.0
        entities, _ = await client.list_entities_services()
        assert "Untouched" not in {entity.name for entity in entities}

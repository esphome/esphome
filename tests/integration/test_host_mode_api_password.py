"""Integration test for API password authentication."""

from __future__ import annotations

import asyncio

from aioesphomeapi import APIConnectionError, InvalidAuthAPIError
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_api_password(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API authentication with password."""
    async with run_compiled(yaml_config):
        # Connect with correct password
        async with api_client_connected(password="test_password_123") as client:
            # Verify we can get device info
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.uses_password is True
            assert device_info.name == "host-mode-api-password"

            # Subscribe to states to ensure authenticated connection works
            loop = asyncio.get_running_loop()
            state_future: asyncio.Future[bool] = loop.create_future()
            states = {}

            def on_state(state):
                states[state.key] = state
                if not state_future.done():
                    state_future.set_result(True)

            client.subscribe_states(on_state)

            # Wait for at least one state with timeout
            try:
                await asyncio.wait_for(state_future, timeout=5.0)
            except TimeoutError:
                pytest.fail("No states received within timeout")

            # Should have received at least one state (the test sensor)
            assert len(states) > 0

        # Test with wrong password - should fail
        # Try connecting with wrong password
        try:
            async with api_client_connected(
                password="wrong_password", timeout=5
            ) as client:
                # If we get here without exception, try to use the connection
                # which should fail if auth failed
                await client.device_info_and_list_entities()
                # If we successfully got device info and entities, auth didn't fail properly
                pytest.fail("Connection succeeded with wrong password")
        except (InvalidAuthAPIError, APIConnectionError) as e:
            # Expected - auth should fail
            # Accept either InvalidAuthAPIError or generic APIConnectionError
            # since the client might not always distinguish
            assert (
                "password" in str(e).lower()
                or "auth" in str(e).lower()
                or "invalid" in str(e).lower()
            )

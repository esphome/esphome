"""Tests for AddressSanitizer and UndefinedBehaviorSanitizer detection."""

from __future__ import annotations

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sanitizer_leak(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that AddressSanitizer catches a deliberate error."""
    # With -fno-sanitize-recover=all and a use-after-free, this should fail with "Sanitizer error detected"
    with pytest.raises((RuntimeError, TimeoutError), match="Sanitizer error detected"):
        async with run_compiled(yaml_config):
            async with api_client_connected() as client:
                await asyncio.sleep(0.5)
                device_info = await client.device_info()
                assert device_info is not None


@pytest.mark.asyncio
async def test_sanitizer_ub(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that UndefinedBehaviorSanitizer catches deliberate UB."""
    # With UBSAN_OPTIONS="halt_on_error=1", this should fail with "Sanitizer error detected"
    # because it happens during setup, before the port is open.
    with pytest.raises((RuntimeError, TimeoutError), match="Sanitizer error detected"):
        async with run_compiled(yaml_config), api_client_connected() as client:
            await asyncio.sleep(0.5)
            device_info = await client.device_info()
            assert device_info is not None

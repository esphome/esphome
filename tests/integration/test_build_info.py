"""Integration test for build_info values."""

from __future__ import annotations

import time

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_build_info(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that build_info values are sane."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "build-info-test"

        # Verify compilation_time is present and reasonable
        # The format is "Mon DD YYYY, HH:MM:SS" (e.g., "Dec 13 2024, 15:30:00")
        compilation_time = device_info.compilation_time
        assert compilation_time is not None
        assert len(compilation_time) > 0, "compilation_time should not be empty"

        # Verify it looks like a date string (contains comma and colon)
        assert "," in compilation_time, (
            f"compilation_time should contain comma: {compilation_time}"
        )
        assert ":" in compilation_time, (
            f"compilation_time should contain colon: {compilation_time}"
        )

        # Verify it contains a year (4 digits)
        import re

        year_match = re.search(r"\b(20\d{2})\b", compilation_time)
        assert year_match is not None, (
            f"compilation_time should contain a year: {compilation_time}"
        )

        # Verify the year is reasonable (within last year to next year)
        year = int(year_match.group(1))
        current_year = time.localtime().tm_year
        assert current_year - 1 <= year <= current_year + 1, (
            f"Year {year} should be close to current year {current_year}"
        )

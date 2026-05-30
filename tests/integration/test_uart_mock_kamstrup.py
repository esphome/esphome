"""Integration test for kamstrup_kmp component with virtual UART.

Tests that crc16be is the correct CRC algorithm for Kamstrup KMP protocol
by verifying that real device frames are parsed successfully end-to-end.

Run with:
    pytest tests/integration/test_uart_mock_kamstrup.py -v
"""

from __future__ import annotations

import pytest

from .state_utils import SensorTracker
from .types import APIClientConnectedFactory, RunCompiledFunction

# Expected values from real Kamstrup device frames (verified via live UART logs).
# Each sensor receives exactly one update per scenario run.
# Tolerance accounts for float precision in C++ → API conversion.
EXPECTED: dict[str, float] = {
    "heat_energy": 216.34,  # 0x003C — energy counter
    "power": 0.3,  # 0x0050 — instantaneous power
    "temperature_1": 51.76,  # 0x0056 — supply temperature
    "temperature_diff": 5.88,  # 0x0059 — delta T
    "flow": 33.0,  # 0x004A — flow rate
    "volume": 1422.53,  # 0x0044 — total volume
    "custom_59": 5.88,  # 0x0059 — custom sensor (same frame as temperature_diff)
}
ATOL = 0.001  # tolerance for float comparison


@pytest.mark.asyncio
async def test_uart_mock_kamstrup(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that kamstrup_kmp correctly parses real device frames using crc16be.

    All frames are captured from live Kamstrup device traffic. This proves
    that crc16be (ESPHome helpers.cpp) is the correct algorithm, not the
    hand-rolled crc16_ccitt that was previously used.
    """

    tracker = SensorTracker(list(EXPECTED.keys()))

    # Build approximate-match futures (SensorTracker.expect uses ==, so we
    # collect states first and assert after).
    # Use expect_any to capture the first update for each sensor.
    futures = {name: tracker.expect_any(name) for name in EXPECTED}

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Wire up subscriptions, wait for initial states, press Start Scenario
        await tracker.setup_and_start_scenario(client)

        # Wait for each sensor to receive at least one update
        await tracker.await_all(futures, timeout=10.0)

        # Now assert approximate equality (float precision from C++ → API)
        for name, expected in EXPECTED.items():
            states = tracker.sensor_states[name]
            assert len(states) >= 1, f"{name}: no state received"
            actual = states[0]
            assert pytest.approx(actual, abs=ATOL) == expected, (
                f"{name}: expected {expected}, got {actual}"
            )

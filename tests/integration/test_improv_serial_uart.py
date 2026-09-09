"""Integration test for improv_serial over a mocked UART bus.

Drives the improv serial protocol end to end on the host platform:
the fixture wires improv_serial to a uart_mock bus and shadows the wifi
component with a host stub. The test injects improv frames through an API
action and asserts on the framed responses that uart_mock logs as TX lines.

Covered:
  1. Get Current State reports AUTHORIZED
  2. Get Device Info returns the firmware/device info RPC response
  3. Get Wi-Fi Networks returns deduplicated scan results and a terminator
  4. Wi-Fi Settings provisions: saves credentials and reports PROVISIONED
"""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

# Improv serial framing (improv_serial_component.h)
IMPROV_HEADER = b"IMPROV"
IMPROV_VERSION = 1
TYPE_CURRENT_STATE = 0x01
TYPE_RPC = 0x03
TYPE_RPC_RESPONSE = 0x04

# improv::Command values
CMD_GET_CURRENT_STATE = 0x02
CMD_GET_DEVICE_INFO = 0x03
CMD_GET_WIFI_NETWORKS = 0x04
CMD_WIFI_SETTINGS = 0x01


def build_rpc_frame(command: int, data: bytes = b"") -> list[int]:
    """Build a full improv serial frame carrying one RPC command."""
    payload = bytes([command, len(data)]) + data
    frame = IMPROV_HEADER + bytes([IMPROV_VERSION, TYPE_RPC, len(payload)]) + payload
    checksum = sum(frame) & 0xFF
    return list(frame + bytes([checksum]) + b"\n")


def state_frame_hex(state: int) -> str:
    """Full 12 byte current-state frame as hex, checksum and newline included."""
    frame = IMPROV_HEADER + bytes([IMPROV_VERSION, TYPE_CURRENT_STATE, 1, state])
    checksum = sum(frame) & 0xFF
    return ":".join(f"{b:02X}" for b in frame + bytes([checksum]) + b"\n")


def rpc_footer_hex(payload: bytes) -> str:
    """Checksum and newline footer written after an RPC response payload."""
    header = IMPROV_HEADER + bytes([IMPROV_VERSION, TYPE_RPC_RESPONSE, len(payload)])
    checksum = (sum(header) + sum(payload)) & 0xFF
    return f"{checksum:02X}:0A"


def wifi_settings_data(ssid: str, password: str) -> bytes:
    ssid_b = ssid.encode()
    pass_b = password.encode()
    return bytes([len(ssid_b)]) + ssid_b + bytes([len(pass_b)]) + pass_b


def hex_of(text: str) -> str:
    """Colon separated uppercase hex as logged by format_hex_pretty."""
    return ":".join(f"{b:02X}" for b in text.encode())


@pytest.mark.asyncio
async def test_improv_serial_uart(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _entities, services = await client.list_entities_services()
        inject = next(s for s in services if s.name == "uart_inject")

        # 1. Get Current State: expect the complete current-state frame reporting
        # AUTHORIZED (0x02), checksum and newline included
        await client.execute_service(
            inject, {"payload": build_rpc_frame(CMD_GET_CURRENT_STATE)}
        )
        await waiter.wait_for("uart_mock", f"TX 12 bytes: {state_frame_hex(0x02)}")

        # 2. Get Device Info: the always logged 9 byte response header, then the
        # payload with the firmware name (must stay under uart_mock's 64 byte
        # hex dump cap or the payload line reads "too large to log")
        await client.execute_service(
            inject, {"payload": build_rpc_frame(CMD_GET_DEVICE_INFO)}
        )
        await waiter.wait_for("uart_mock", "TX 9 bytes: 49:4D:50:52:4F:56:01:04")
        await waiter.wait_for("uart_mock", "TX ", hex_of("ESPHome"))

        # 3. Get Wi-Fi Networks: stub scan has TestNet twice (dedup keeps the
        # stronger), OpenNet, and a hidden entry (filtered). Expect one response
        # per visible network plus the empty terminator.
        await client.execute_service(
            inject, {"payload": build_rpc_frame(CMD_GET_WIFI_NETWORKS)}
        )
        await waiter.wait_for("uart_mock", hex_of("TestNet"))
        await waiter.wait_for("uart_mock", hex_of("OpenNet"))
        # Terminator: all three writes of the response frame; 9 byte header,
        # payload [0x04, 0x00, 0x00], then the checksum and newline footer
        await waiter.wait_for("uart_mock", "TX 9 bytes: 49:4D:50:52:4F:56:01:04:03")
        await waiter.wait_for("uart_mock", "TX 3 bytes: 04:00:00")
        await waiter.wait_for(
            "uart_mock", f"TX 2 bytes: {rpc_footer_hex(bytes([0x04, 0x00, 0x00]))}"
        )
        testnet_count = sum(
            1
            for line in waiter.lines
            if "uart_mock" in line and "TX " in line and hex_of("TestNet") in line
        )
        assert testnet_count == 1, (
            f"Duplicate scan entry not deduplicated: {testnet_count} TestNet responses"
        )

        # 4. Wi-Fi Settings: stub connects immediately; expect the credentials
        # saved, the PROVISIONED state frame (0x04), and the settings response
        await client.execute_service(
            inject,
            {
                "payload": build_rpc_frame(
                    CMD_WIFI_SETTINGS, wifi_settings_data("NewNet", "secret123")
                )
            },
        )
        await waiter.wait_for("save_wifi_sta ssid=NewNet")
        await waiter.wait_for("uart_mock", f"TX 12 bytes: {state_frame_hex(0x04)}")
        # Settings RPC response carries the formatted next_url and its footer
        next_url = b"https://example.com/?device=improv-uart"
        payload = (
            bytes([CMD_WIFI_SETTINGS, len(next_url) + 1, len(next_url)])
            + next_url
            + b"\x00"
        )
        await waiter.wait_for(
            "uart_mock",
            f"TX {len(payload)} bytes: " + ":".join(f"{b:02X}" for b in payload),
        )
        await waiter.wait_for("uart_mock", f"TX 2 bytes: {rpc_footer_hex(payload)}")

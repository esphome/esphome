"""A client whose send path is full at shutdown still gets DisconnectRequest.

The disconnect then waits in an otherwise empty deferred batch. Teardown does not
advance the cached loop time that the batch_delay gate reads, so unless the batch
is made due at once it is never sent before the reboot.
"""

from __future__ import annotations

import asyncio
import sys

from aioesphomeapi import api_pb2
import pytest

from .log_utils import LineWaiter
from .raw_api_client import (
    MESSAGE_TYPE_OF,
    RECV_BUFFER_SIZE,
    SNDBUF_PIN_LOG,
    RawApiClient,
    pin_server_sndbuf,
)
from .types import RunCompiledFunction

DISCONNECT_REQUEST = MESSAGE_TYPE_OF[api_pb2.DisconnectRequest]
SERVICES_RESPONSE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesServicesResponse]
LIST_DONE_RESPONSE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesDoneResponse]
SENSOR_STATE = MESSAGE_TYPE_OF[api_pb2.SensorStateResponse]

LINUX_ONLY = pytest.mark.skipif(
    sys.platform != "linux",
    reason="other kernels grow loopback buffers past the pinned size, so the pipe never fills",
)


async def _read_until_disconnect_or_closed(client: RawApiClient) -> None:
    while not client.frame_counts[DISCONNECT_REQUEST]:
        try:
            await client._recv_once()
        except (AssertionError, ConnectionError):
            return


async def _fill_then_restart(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    port: int,
    action: str,
    subscribe_states: bool,
) -> RawApiClient:
    """Run ``action`` with the client not reading, then read again during teardown."""
    waiter = LineWaiter()
    async with run_compiled(
        pin_server_sndbuf(yaml_config), line_callback=waiter.callback
    ):
        await waiter.wait_for(SNDBUF_PIN_LOG, timeout=10)

        async with RawApiClient(port, recv_buffer_size=RECV_BUFFER_SIZE) as client:
            await client.connect(client_info="shutdown-backpressure-client")
            await client.send_message(api_pb2.ListEntitiesRequest())
            await client.read_until_frame(LIST_DONE_RESPONSE)
            services = [
                api_pb2.ListEntitiesServicesResponse.FromString(payload)
                for payload in client.frames(SERVICES_RESPONSE)
            ]
            (action_key,) = [svc.key for svc in services if svc.name == action]
            if subscribe_states:
                await client.send_message(api_pb2.SubscribeStatesRequest())
                # Take the initial state so nothing is queued before the fill
                await client.read_until_frame(SENSOR_STATE)
            await client.send_message(
                api_pb2.SubscribeLogsRequest(level=api_pb2.LOG_LEVEL_DEBUG)
            )

            # The client stops reading here; the device console tells when the pipe is full
            await client.send_message(api_pb2.ExecuteServiceRequest(key=action_key))
            await waiter.wait_for("fill done", timeout=10)
            # Logged just before the shutdown hooks; reading earlier would let the direct send succeed
            await waiter.wait_for("Rebooting safely", timeout=10)
            assert not any("fill timed out" in line for line in waiter.lines), (
                "the client's receive window never closed while the overflow buffer held data"
            )

            # Read again during the 1 s teardown; the disconnect must arrive before the reboot
            await asyncio.wait_for(_read_until_disconnect_or_closed(client), 10)
            return client


@LINUX_ONLY
@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("action", "subscribe_states"),
    [
        ("fill_then_restart", False),
        # A state queued behind the blocked socket when the shutdown starts
        ("fill_publish_then_restart", True),
    ],
)
async def test_api_shutdown_disconnect_backpressure(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
    action: str,
    subscribe_states: bool,
) -> None:
    client = await _fill_then_restart(
        yaml_config, run_compiled, unused_tcp_port, action, subscribe_states
    )
    assert client.frame_counts[DISCONNECT_REQUEST] == 1, (
        "the backed up client never got DisconnectRequest"
    )

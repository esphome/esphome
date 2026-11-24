"""Events of one entity must reach the client in the order they fired.

Under backpressure an event waits in the deferred batch for batch_delay. The
overflow buffer drains earlier: at the start of each api loop pass, and
whenever a log line is sent. A newer event of the same entity must not then
go out at once and overtake the waiting one, or Home Assistant ends on the
older event type.

The fixture's batch_delay is 10 s, so "first" only arrives in time if a queued
event makes the batch due at once.
"""

from __future__ import annotations

import contextlib
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

EVENT_RESPONSE = MESSAGE_TYPE_OF[api_pb2.EventResponse]
LOG_RESPONSE = MESSAGE_TYPE_OF[api_pb2.SubscribeLogsResponse]
SERVICES_RESPONSE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesServicesResponse]
LIST_DONE_RESPONSE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesDoneResponse]

# Half the fixture's batch_delay of 10 s
EVENT_TIMEOUT = 5.0


@pytest.mark.skipif(
    sys.platform != "linux",
    reason="other kernels grow loopback buffers past the pinned size, so the pipe never fills",
)
@pytest.mark.asyncio
async def test_api_event_order_backpressure(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    """A newer event must not overtake an older one of the same entity."""
    config = pin_server_sndbuf(yaml_config)

    waiter = LineWaiter()
    async with run_compiled(config, line_callback=waiter.callback):
        await waiter.wait_for(SNDBUF_PIN_LOG, timeout=10)

        async with RawApiClient(
            unused_tcp_port, recv_buffer_size=RECV_BUFFER_SIZE
        ) as client:
            await client.connect(client_info="event-order-client")
            await client.send_message(api_pb2.ListEntitiesRequest())
            await client.read_until_frame(LIST_DONE_RESPONSE)
            (action_payload,) = client.frames(SERVICES_RESPONSE)
            action_key = api_pb2.ListEntitiesServicesResponse.FromString(
                action_payload
            ).key
            await client.send_message(api_pb2.SubscribeStatesRequest())
            await client.send_message(
                api_pb2.SubscribeLogsRequest(level=api_pb2.LOG_LEVEL_DEBUG)
            )

            # The client stops reading here, so the device console tells when "first" fired
            await client.send_message(api_pb2.ExecuteServiceRequest(key=action_key))
            await waiter.wait_for("first fired", timeout=10)
            assert not any("fill timed out" in line for line in waiter.lines), (
                "the client's receive window never closed while the overflow buffer held data"
            )

            # The client reads again while the action still runs.
            # On a timeout the asserts below show which event is missing
            with contextlib.suppress(TimeoutError):
                await client.read_until_frame(
                    EVENT_RESPONSE, timeout=EVENT_TIMEOUT, count=2
                )

            # A dropped last fill line proves the overflow buffer held data when "first" fired
            log_lines = [
                api_pb2.SubscribeLogsResponse.FromString(payload).message
                for payload in client.frames(LOG_RESPONSE)
            ]
            assert not any(b"last fill line" in line for line in log_lines), (
                "the pipe was not full when first fired"
            )
            # A received last flush line proves the overflow buffer was empty before "second" fired
            assert any(b"last flush line" in line for line in log_lines), (
                "the overflow buffer still held data when second fired"
            )

            event_types = [
                api_pb2.EventResponse.FromString(payload).event_type
                for payload in client.frames(EVENT_RESPONSE)
            ]
            assert event_types == ["first", "second"], (
                "a newer event overtook the older one, or one waited for batch_delay"
            )

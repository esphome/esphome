"""A serial proxy must not lose bytes while its client is not reading.

The proxy forwards what it reads from the UART in api messages. When the TCP
pipe to the client is full, the api refuses the message; bytes already read
from the UART would then be gone, leaving holes in the client's stream. The
proxy leaves them in the UART's buffer until the connection can send again.

The sndbuf_pin_component fixture pins the device's send buffers so the pipe
fills deterministically regardless of kernel autotuning; the test waits for
its log line before proceeding.
"""

from __future__ import annotations

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

DATA_RECEIVED = MESSAGE_TYPE_OF[api_pb2.SerialProxyDataReceived]

# Injected by the fixture every 20 ms; no more than the proxy reads in one loop pass
STREAM_CHUNK = 200
# Sent once the client's connection is blocked
STREAM_BYTES = 10_000
# A prime, so the pattern does not line up with the chunk or with the proxy's read size
STREAM_MODULUS = 251
# Sent until the connection is blocked; never part of the numbered bytes
STREAM_FILLER = 0xFF
assert STREAM_BYTES % STREAM_CHUNK == 0
assert STREAM_FILLER >= STREAM_MODULUS


@pytest.mark.skipif(
    sys.platform != "linux",
    reason="other kernels grow loopback buffers past the pinned size, so the pipe never fills",
)
@pytest.mark.asyncio
async def test_serial_proxy_backpressure(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    """Every byte the device sent while the client stalled arrives, in order."""
    config = pin_server_sndbuf(yaml_config)
    config = config.replace("STREAM_BYTES", str(STREAM_BYTES))
    config = config.replace("STREAM_CHUNK", str(STREAM_CHUNK))
    config = config.replace("STREAM_MODULUS", str(STREAM_MODULUS))
    config = config.replace("STREAM_FILLER", str(STREAM_FILLER))

    waiter = LineWaiter()
    async with run_compiled(config, line_callback=waiter.callback):
        # Fails loudly if the pin never applied
        await waiter.wait_for(SNDBUF_PIN_LOG, timeout=10)

        async with RawApiClient(
            unused_tcp_port, recv_buffer_size=RECV_BUFFER_SIZE
        ) as client:
            await client.connect(client_info="serial-proxy-stall-client")
            subscribe = api_pb2.SerialProxyRequest()
            subscribe.instance = 0
            subscribe.type = api_pb2.SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE
            await client.send_message(subscribe)
            # The client now stops reading entirely. The device fills the pipe, then sends
            # the numbered bytes into the blocked connection
            await waiter.wait_for("stream complete", timeout=120)

            received = bytearray()
            try:
                while len(received) < STREAM_BYTES:
                    frame = await client.read_frame(DATA_RECEIVED, timeout=10)
                    data = api_pb2.SerialProxyDataReceived.FromString(frame).data
                    received += data.replace(bytes([STREAM_FILLER]), b"")
            except TimeoutError:
                pytest.fail(
                    f"only {len(received)} of {STREAM_BYTES} bytes arrived; "
                    "the proxy lost data while the client was not reading"
                )

            expected = bytes(i % STREAM_MODULUS for i in range(STREAM_BYTES))
            assert bytes(received) == expected

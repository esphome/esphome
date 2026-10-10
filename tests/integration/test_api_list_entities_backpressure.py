"""A client that stops reading the entity listing must not starve other clients.

Service responses are sent directly (not via the deferred batch), so a full
TCP pipe makes the send path refuse; the drive loop now lives in
try_advance(), which stops on refusal instead of retrying forever. Not a
before/after regression test: pre-fix builds survive here because the
refusal path yields and pumps the socket each retry.

The sndbuf_pin_component fixture pins the device's send buffers so the pipe
fills deterministically regardless of kernel autotuning; the test waits for
its log line before proceeding.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import api_pb2
import pytest

from .raw_api_client import MESSAGE_TYPE_OF, RawApiClient
from .types import APIClientConnectedFactory, RunCompiledFunction

SERVICES_RESPONSE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesServicesResponse]
LIST_DONE_RESPONSE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesDoneResponse]

# Both ends of the pipe are pinned small; only tens of KB fit in the kernel
RECV_BUFFER_SIZE = 4096
SERVER_SNDBUF = 8192  # substituted into the fixture yaml
# Logged by the sndbuf_pin_component fixture when it pins a socket
SNDBUF_PIN_LOG = "SO_SNDBUF pinned to"
# One response (~6.4 KB) must stay smaller than the pinned send buffer; an
# oversized message parks in the overflow buffer and reports as sent.
ARGS_PER_SERVICE = 8
ARG_NAME_LEN = 800
# ~160 KB listing versus a tens-of-KB pipe guarantees a mid-services block
NUM_SERVICES = 25
assert ARGS_PER_SERVICE * ARG_NAME_LEN < SERVER_SNDBUF
# The pipe fills in well under a second
STALL_SECONDS = 0.5
# Well above pipe capacity, well below the listing size
MIN_DRAINED_BYTES = 60_000


def _generated_actions() -> str:
    """Build the api actions block: services with long argument names."""
    lines: list[str] = []
    for i in range(NUM_SERVICES):
        lines.append(f"    - action: backpressure_service_{i:04d}")
        lines.append("      variables:")
        for j in range(ARGS_PER_SERVICE):
            prefix = f"arg_{i:04d}_{j:02d}_"
            lines.append(
                f"        {prefix}{'x' * (ARG_NAME_LEN - len(prefix))}: string"
            )
        lines.append("      then:")
        lines.append("        - logger.log: service called")
    return "\n".join(lines)


@pytest.mark.asyncio
async def test_api_list_entities_backpressure(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port: int,
) -> None:
    """A stalled reader mid-services must not block other api clients."""
    assert "# GENERATED_ACTIONS" in yaml_config
    config = yaml_config.replace("# GENERATED_ACTIONS", _generated_actions())
    config = config.replace("SERVER_SNDBUF", str(SERVER_SNDBUF))

    pin_applied = asyncio.Event()

    def _on_log_line(line: str) -> None:
        if SNDBUF_PIN_LOG in line:
            pin_applied.set()

    async with run_compiled(config, line_callback=_on_log_line):
        # Fails loudly if the pin never applied
        await asyncio.wait_for(pin_applied.wait(), 10)

        async with RawApiClient(
            unused_tcp_port, recv_buffer_size=RECV_BUFFER_SIZE
        ) as stalled:
            await stalled.connect(client_info="backpressure-stall-client")
            await stalled.send_message(api_pb2.ListEntitiesRequest())
            # The client now stops reading entirely.

            # Let the server run against the full pipe
            await asyncio.sleep(STALL_SECONDS)

            # Other clients must still be served while the first is blocked
            async with api_client_connected(timeout=20) as client:
                device_info = await asyncio.wait_for(client.device_info(), 20)
                assert device_info.name == "api-backpressure-test"
                _, services = await asyncio.wait_for(
                    client.list_entities_services(), 30
                )
                assert len(services) == NUM_SERVICES

            # Fixture-size guard: the listing must dwarf the pinned pipe
            before = stalled.bytes_received
            await stalled.read_until_frame(LIST_DONE_RESPONSE, timeout=60)
            drained = stalled.bytes_received - before
            assert drained > MIN_DRAINED_BYTES, (
                f"only {drained} bytes drained; the listing never backed up"
            )
            assert stalled.frame_counts[SERVICES_RESPONSE] == NUM_SERVICES
            assert stalled.frame_counts[LIST_DONE_RESPONSE] == 1

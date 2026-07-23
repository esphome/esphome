"""Exercise ESP-IDF HTTPD ownership after a stalled SSE consumer.

Run this against a device that includes ``sse_lifecycle_hil_package.yaml``.
The client intentionally stops reading /events, then verifies that the server
closes the TCP session and remains reachable. No background process is used.
"""

from __future__ import annotations

import argparse
import base64
from dataclasses import dataclass
import getpass
import hashlib
import json
import os
import re
import secrets
import socket
import time

HEADER_LIMIT = 64 * 1024


@dataclass(frozen=True)
class Credentials:
    username: str
    password: str


@dataclass
class HttpResponse:
    sock: socket.socket
    status: int
    headers: dict[str, str]
    initial_body: bytes


def _md5(value: str) -> str:
    return hashlib.md5(value.encode(), usedforsecurity=False).hexdigest()


def _parse_digest_challenge(value: str) -> dict[str, str]:
    if not value.lower().startswith("digest "):
        raise RuntimeError("server did not offer Digest authentication")
    params: dict[str, str] = {}
    pattern = re.compile(r'(\w+)=(?:"([^"]*)"|([^,\s]+))')
    for match in pattern.finditer(value[7:]):
        params[match.group(1).lower()] = match.group(2) or match.group(3)
    if "realm" not in params or "nonce" not in params:
        raise RuntimeError("incomplete Digest challenge")
    return params


def _digest_authorization(credentials: Credentials, challenge: str, path: str) -> str:
    params = _parse_digest_challenge(challenge)
    algorithm = params.get("algorithm", "MD5").upper()
    if algorithm != "MD5":
        raise RuntimeError(f"unsupported Digest algorithm: {algorithm}")

    qop_values = [item.strip() for item in params.get("qop", "auth").split(",")]
    if "auth" not in qop_values:
        raise RuntimeError("server did not offer Digest qop=auth")
    qop = "auth"
    nonce_count = "00000001"
    cnonce = secrets.token_hex(8)
    ha1 = _md5(f"{credentials.username}:{params['realm']}:{credentials.password}")
    ha2 = _md5(f"GET:{path}")
    response = _md5(f"{ha1}:{params['nonce']}:{nonce_count}:{cnonce}:{qop}:{ha2}")

    fields = [
        f'username="{credentials.username}"',
        f'realm="{params["realm"]}"',
        f'nonce="{params["nonce"]}"',
        f'uri="{path}"',
        f'response="{response}"',
        f"qop={qop}",
        f"nc={nonce_count}",
        f'cnonce="{cnonce}"',
    ]
    if opaque := params.get("opaque"):
        fields.append(f'opaque="{opaque}"')
    return "Digest " + ", ".join(fields)


def _basic_authorization(credentials: Credentials) -> str:
    token = base64.b64encode(
        f"{credentials.username}:{credentials.password}".encode()
    ).decode()
    return f"Basic {token}"


def _authorization(credentials: Credentials, challenge: str, path: str) -> str:
    scheme = challenge.split(" ", 1)[0].lower()
    if scheme == "basic":
        return _basic_authorization(credentials)
    if scheme == "digest":
        return _digest_authorization(credentials, challenge, path)
    raise RuntimeError(f"unsupported authentication scheme: {scheme}")


def _read_response_headers(sock: socket.socket) -> tuple[int, dict[str, str], bytes]:
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed before HTTP response headers")
        data.extend(chunk)
        if len(data) > HEADER_LIMIT:
            raise RuntimeError("HTTP response headers exceeded safety limit")

    raw_headers, initial_body = bytes(data).split(b"\r\n\r\n", 1)
    lines = raw_headers.decode("iso-8859-1").split("\r\n")
    parts = lines[0].split(" ", 2)
    if len(parts) < 2:
        raise RuntimeError(f"invalid HTTP status line: {lines[0]!r}")
    headers: dict[str, str] = {}
    for line in lines[1:]:
        name, separator, value = line.partition(":")
        if separator:
            headers[name.lower()] = value.strip()
    return int(parts[1]), headers, initial_body


def _open_http(
    host: str,
    port: int,
    path: str,
    credentials: Credentials | None,
    *,
    receive_buffer: int | None = None,
) -> HttpResponse:
    authorization: str | None = None
    for attempt in range(2):
        sock = socket.create_connection((host, port), timeout=5)
        sock.settimeout(5)
        if receive_buffer is not None:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, receive_buffer)
        request_headers = [
            f"GET {path} HTTP/1.1",
            f"Host: {host}",
            "Accept: text/event-stream"
            if path == "/events"
            else "Accept: application/json, text/html",
            "Connection: keep-alive" if path == "/events" else "Connection: close",
        ]
        if authorization is not None:
            request_headers.append(f"Authorization: {authorization}")
        sock.sendall(("\r\n".join(request_headers) + "\r\n\r\n").encode())
        status, headers, initial_body = _read_response_headers(sock)
        if status != 401:
            return HttpResponse(sock, status, headers, initial_body)

        challenge = headers.get("www-authenticate")
        sock.close()
        if attempt != 0 or credentials is None or challenge is None:
            raise RuntimeError("HTTP authentication failed")
        authorization = _authorization(credentials, challenge, path)
    raise AssertionError("unreachable")


def _read_to_close(response: HttpResponse, timeout: float) -> bytes:
    body = bytearray(response.initial_body)
    deadline = time.monotonic() + timeout
    response.sock.settimeout(0.25)
    while time.monotonic() < deadline:
        try:
            chunk = response.sock.recv(4096)
        except TimeoutError:
            continue
        if not chunk:
            return bytes(body)
        body.extend(chunk)
    raise RuntimeError("HTTP response did not close before timeout")


def _read_body(response: HttpResponse, timeout: float) -> bytes:
    content_length = response.headers.get("content-length")
    if content_length is None:
        return _read_to_close(response, timeout)

    expected = int(content_length)
    body = bytearray(response.initial_body)
    deadline = time.monotonic() + timeout
    response.sock.settimeout(0.25)
    while len(body) < expected and time.monotonic() < deadline:
        try:
            body.extend(response.sock.recv(expected - len(body)))
        except TimeoutError:
            continue
    if len(body) < expected:
        raise RuntimeError("HTTP response body did not complete before timeout")
    return bytes(body[:expected])


def _request(
    host: str, port: int, path: str, credentials: Credentials | None
) -> tuple[int, dict[str, str], bytes]:
    response = _open_http(host, port, path, credentials)
    try:
        body = _read_body(response, 5)
        return response.status, response.headers, body
    finally:
        response.sock.close()


def _wait_for_server_close(response: HttpResponse, timeout: float) -> int:
    drained = len(response.initial_body)
    deadline = time.monotonic() + timeout
    response.sock.settimeout(0.25)
    while time.monotonic() < deadline:
        try:
            chunk = response.sock.recv(4096)
        except TimeoutError:
            continue
        if not chunk:
            return drained
        drained += len(chunk)
    raise RuntimeError("stalled EventSource TCP session remained open")


def _sample_heap(
    host: str, port: int, path: str | None, credentials: Credentials | None
) -> int | None:
    if path is None:
        return None
    status, _, body = _request(host, port, path, credentials)
    if status != 200:
        raise RuntimeError(f"heap endpoint returned HTTP {status}")
    payload = json.loads(body)
    value = payload.get("value", payload.get("state"))
    if value is None:
        raise RuntimeError("heap endpoint JSON has no value/state field")
    return int(float(value))


def _check_headroom(
    host: str,
    port: int,
    credentials: Credentials | None,
    connections: int,
) -> None:
    streams: list[HttpResponse] = []
    try:
        for _ in range(connections):
            stream = _open_http(host, port, "/events", credentials)
            if stream.status != 200:
                raise RuntimeError(
                    f"headroom EventSource returned HTTP {stream.status}"
                )
            streams.append(stream)
        status, _, _ = _request(host, port, "/", credentials)
        if status != 200:
            raise RuntimeError(f"health request returned HTTP {status}")
        for index, stream in enumerate(streams, start=1):
            _verify_stream_still_open(stream, index)
    finally:
        for stream in streams:
            stream.sock.close()


def _verify_stream_still_open(response: HttpResponse, index: int) -> None:
    deadline = time.monotonic() + 1.0
    response.sock.settimeout(0.1)
    while time.monotonic() < deadline:
        try:
            chunk = response.sock.recv(4096)
        except TimeoutError:
            continue
        if not chunk:
            raise RuntimeError(
                f"headroom EventSource {index} closed during health probe"
            )


def _credentials(args: argparse.Namespace) -> Credentials | None:
    if args.username is None:
        return None
    password = os.environ.get(args.password_env)
    if password is None:
        password = getpass.getpass("Web password: ")
    return Credentials(args.username, password)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=80)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--stall-seconds", type=float, default=30)
    parser.add_argument("--close-timeout", type=float, default=5)
    parser.add_argument("--receive-buffer", type=int, default=1024)
    parser.add_argument("--headroom-connections", type=int, default=5)
    parser.add_argument("--heap-path")
    parser.add_argument("--max-heap-drop", type=int, default=8192)
    parser.add_argument("--username")
    parser.add_argument("--password-env", default="ESPHOME_WEB_PASSWORD")
    args = parser.parse_args()
    credentials = _credentials(args)

    heap_samples: list[int] = []
    first_heap = _sample_heap(args.host, args.port, args.heap_path, credentials)
    if first_heap is not None:
        heap_samples.append(first_heap)

    for cycle in range(1, args.repeats + 1):
        stream = _open_http(
            args.host,
            args.port,
            "/events",
            credentials,
            receive_buffer=args.receive_buffer,
        )
        if stream.status != 200:
            stream.sock.close()
            raise RuntimeError(f"EventSource returned HTTP {stream.status}")

        try:
            time.sleep(args.stall_seconds)
            drained = _wait_for_server_close(stream, args.close_timeout)
        finally:
            stream.sock.close()

        status, _, _ = _request(args.host, args.port, "/", credentials)
        if status != 200:
            raise RuntimeError(f"post-close health request returned HTTP {status}")

        fresh = _open_http(args.host, args.port, "/events", credentials)
        try:
            if fresh.status != 200:
                raise RuntimeError(f"fresh EventSource returned HTTP {fresh.status}")
        finally:
            fresh.sock.close()

        _check_headroom(args.host, args.port, credentials, args.headroom_connections)
        heap = _sample_heap(args.host, args.port, args.heap_path, credentials)
        if heap is not None:
            heap_samples.append(heap)
        print(
            f"cycle {cycle}/{args.repeats}: closed, drained={drained} bytes, heap={heap}"
        )

    if heap_samples and heap_samples[0] - min(heap_samples) > args.max_heap_drop:
        raise RuntimeError(
            f"heap dropped by {heap_samples[0] - min(heap_samples)} bytes "
            f"(limit {args.max_heap_drop})"
        )
    print(
        "PASS: stalled sessions closed; HTTP, SSE, slot headroom, and heap checks passed"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Common fixtures for integration tests."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncGenerator, Callable, Generator
from contextlib import AbstractAsyncContextManager, asynccontextmanager
import fcntl
import hashlib
import logging
import os
from pathlib import Path
import platform
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
from typing import TextIO

from aioesphomeapi import APIClient, APIConnectionError, LogParser, ReconnectLogic
import pytest
import pytest_asyncio

import esphome.config
from esphome.core import CORE
from esphome.helpers import get_usable_cpu_count
from esphome.platformio.toolchain import get_idedata

from .const import (
    API_CONNECTION_TIMEOUT,
    DEFAULT_API_PORT,
    LOCALHOST,
    PORT_POLL_INTERVAL,
    PORT_WAIT_TIMEOUT,
    SIGINT_TIMEOUT,
    SIGTERM_TIMEOUT,
)
from .types import (
    APIClientConnectedFactory,
    APIClientFactory,
    CompileFunction,
    ConfigWriter,
    RunCompiledFunction,
)

# Skip all integration tests on Windows
if platform.system() == "Windows":
    pytest.skip(
        "Integration tests are not supported on Windows", allow_module_level=True
    )


import pty  # not available on Windows

# Register assert rewrite for entity_utils so assertions have proper error messages
pytest.register_assert_rewrite("tests.integration.entity_utils")


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers",
        "shared_yaml(name): load fixtures/<name>.yaml and compile it in a shared, "
        "hash-keyed incremental build directory",
    )


FIXTURES_DIR = Path(__file__).parent / "fixtures"
REPO_ROOT = Path(__file__).resolve().parent.parent.parent

# CI caches parts of this path; keep in sync with ci.yml integration-tests.
INTEGRATION_TESTS_ROOT = Path.home() / ".esphome-integration-tests"


def _get_platformio_env(cache_dir: Path) -> dict[str, str]:
    """Get environment variables for PlatformIO with shared cache."""
    env = os.environ.copy()
    env["PLATFORMIO_CORE_DIR"] = str(cache_dir)
    env["PLATFORMIO_CACHE_DIR"] = str(cache_dir / ".cache")
    # libdeps is keyed only by env name (the device name), and fixtures share
    # names; two xdist workers first-compiling the same name race pio pkg
    # install in the same directory. Keep libdeps per worker.
    worker = os.environ.get("PYTEST_XDIST_WORKER", "master")
    env["PLATFORMIO_LIBDEPS_DIR"] = str(cache_dir / "libdeps" / worker)
    # Prevent cache cleaning during integration tests
    env["ESPHOME_SKIP_CLEAN_BUILD"] = "1"
    # Cap each compile's -j so several xdist workers do not each spawn a
    # full-width compiler fan-out on the same machine. An explicit env wins.
    if "ESPHOME_DEFAULT_COMPILE_PROCESS_LIMIT" not in os.environ:
        workers = int(os.environ.get("PYTEST_XDIST_WORKER_COUNT", "1"))
        # Floor of 2 keeps a lone tail compile from running fully serial
        env["ESPHOME_DEFAULT_COMPILE_PROCESS_LIMIT"] = str(
            max(2, get_usable_cpu_count() // workers)
        )
    # Compile with THIS tree's esphome sources, not wherever the venv's editable
    # install points (which may be a different git worktree or checkout).
    repo_root = str(REPO_ROOT)
    existing = env.get("PYTHONPATH")
    env["PYTHONPATH"] = f"{repo_root}{os.pathsep}{existing}" if existing else repo_root
    return env


@pytest.fixture(scope="session")
def shared_platformio_cache() -> Generator[Path]:
    """Initialize a shared PlatformIO cache for all integration tests."""
    # Use a dedicated directory for integration tests to avoid conflicts.
    test_cache_dir = INTEGRATION_TESTS_ROOT
    cache_dir = test_cache_dir / "platformio"

    # Use a lock file in the home directory to ensure only one process initializes the cache
    # This is needed when running with pytest-xdist
    # The lock file must be in a directory that already exists to avoid race conditions
    lock_file = Path.home() / ".esphome-integration-tests-init.lock"

    # Always acquire the lock to ensure cache is ready before proceeding
    with lock_file.open("w") as lock_fd:
        fcntl.flock(lock_fd.fileno(), fcntl.LOCK_EX)

        # Check if the native platform is installed (the actual indicator of a populated cache)
        native_platform = cache_dir / "platforms" / "native"
        if not native_platform.exists():
            # Create the test cache directory if it doesn't exist
            test_cache_dir.mkdir(exist_ok=True)

            with tempfile.TemporaryDirectory() as tmpdir:
                # Use the cache_init fixture for initialization
                init_dir = Path(tmpdir)
                fixture_path = Path(__file__).parent / "fixtures" / "cache_init.yaml"
                config_path = init_dir / "cache_init.yaml"
                config_path.write_text(fixture_path.read_text())

                # Run compilation to populate the cache
                # We must succeed here to avoid race conditions where multiple
                # tests try to populate the same cache directory simultaneously
                env = _get_platformio_env(cache_dir)

                subprocess.run(
                    [sys.executable, "-m", "esphome", "compile", str(config_path)],
                    check=True,
                    cwd=init_dir,
                    env=env,
                    close_fds=False,
                )

        # Lock is held until here, ensuring cache is fully populated before any test proceeds

    yield cache_dir


@pytest.fixture(scope="module", autouse=True)
def enable_aioesphomeapi_debug_logging():
    """Enable debug logging for aioesphomeapi to help diagnose connection issues."""
    # Get the aioesphomeapi logger
    logger = logging.getLogger("aioesphomeapi")
    # Save the original level
    original_level = logger.level
    # Set to DEBUG level
    logger.setLevel(logging.DEBUG)
    # Also ensure we have a handler that outputs to console
    if not logger.handlers:
        handler = logging.StreamHandler()
        handler.setLevel(logging.DEBUG)
        formatter = logging.Formatter(
            "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
        )
        handler.setFormatter(formatter)
        logger.addHandler(handler)
    yield
    # Restore original level
    logger.setLevel(original_level)


@pytest.fixture
def integration_test_dir() -> Generator[Path]:
    """Create a temporary directory for integration tests."""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)


@pytest.fixture
def reserved_tcp_port() -> Generator[tuple[int, socket.socket]]:
    """Reserve an unused TCP port by holding the socket open."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", 0))
    port = s.getsockname()[1]
    try:
        yield port, s
    finally:
        s.close()


@pytest.fixture
def unused_tcp_port(reserved_tcp_port: tuple[int, socket.socket]) -> int:
    """Get the reserved TCP port number."""
    return reserved_tcp_port[0]


@pytest_asyncio.fixture
async def yaml_config(request: pytest.FixtureRequest, unused_tcp_port: int) -> str:
    """Load YAML configuration based on test name."""
    # Base test name: test_ prefix and any parametrization stripped
    base_name = (
        _shared_yaml_name(request)
        or request.node.name.replace("test_", "").partition("[")[0]
    )

    # Load the fixture file
    fixture_path = FIXTURES_DIR / f"{base_name}.yaml"
    if not fixture_path.exists():
        raise FileNotFoundError(f"Fixture file not found: {fixture_path}")

    loop = asyncio.get_running_loop()
    content = await loop.run_in_executor(None, fixture_path.read_text)

    # Replace the port in the config if it contains api section
    if "api:" in content:
        # Add port configuration after api:
        content = content.replace("api:", f"api:\n  port: {unused_tcp_port}")

    # Add debug build flags for integration tests to enable assertions
    if "esphome:" in content and "platformio_options:" not in content:
        # Add platformio_options with debug flags after esphome:
        content = content.replace(
            "esphome:",
            "esphome:\n"
            "  # Enable assertions for integration tests\n"
            "  platformio_options:\n"
            "    build_flags:\n"
            '      - "-DDEBUG"  # Enable assert() statements\n'
            '      - "-DESPHOME_DEBUG"  # Enable ESPHOME_DEBUG_ASSERT checks\n'
            '      - "-DESPHOME_DEBUG_API"  # Enable API protocol asserts\n'
            '      - "-g"       # Add debug symbols',
        )

    # Replace external component path placeholder if present
    if "EXTERNAL_COMPONENT_PATH" in content:
        external_components_path = str(FIXTURES_DIR / "external_components")
        content = content.replace("EXTERNAL_COMPONENT_PATH", external_components_path)

    if _shared_yaml_name(request) is not None:
        # _compile verifies the marked test compiles this content unmodified
        request.node._shared_yaml_content = content

    return content


@pytest_asyncio.fixture
async def write_yaml_config(
    integration_test_dir: Path, request: pytest.FixtureRequest
) -> AsyncGenerator[ConfigWriter]:
    """Write YAML configuration to a file."""
    # Get the test name for default filename
    test_name = request.node.name
    base_name = test_name.replace("test_", "").split("[")[0]

    async def _write_config(content: str, filename: str | None = None) -> Path:
        if filename is None:
            filename = f"{base_name}.yaml"
        config_path = integration_test_dir / filename
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, config_path.write_text, content)
        return config_path

    yield _write_config


# Deliberately not CI-cached (ci.yml caches only platformio/ subpaths); stale
# dirs for a fixture are pruned when its content hash changes.
SHARED_BUILDS_ROOT = INTEGRATION_TESTS_ROOT / "builds"

# ELF path per shared build dir; constant once compiled, so resolve it only once
_shared_elf_paths: dict[Path, Path] = {}


def _shared_yaml_name(request: pytest.FixtureRequest) -> str | None:
    """Name passed to the shared_yaml marker, or None when unmarked."""
    marker = request.node.get_closest_marker("shared_yaml")
    return marker.args[0] if marker is not None else None


# In the dir name (not just the hash) so pruning stays inside this checkout
_REPO_KEY = hashlib.sha256(str(REPO_ROOT).encode()).hexdigest()[:8]

# Give a contended shared build lock time for a full cold compile ahead of us
_SHARED_LOCK_TIMEOUT_S = 900


def _shared_build_key(name: str) -> str:
    """Key shared build dirs by the fixture source, before per-test injections."""
    return hashlib.sha256((FIXTURES_DIR / f"{name}.yaml").read_bytes()).hexdigest()[:16]


def _prune_stale_builds(name: str, keep: Path) -> None:
    """Remove this checkout's outdated build dirs for a fixture (blocking, run
    in executor). Tolerates other workers pruning the same dirs concurrently."""
    for stale in SHARED_BUILDS_ROOT.glob(f"{name}-{_REPO_KEY}-*"):
        if stale == keep:
            continue
        try:
            lock_file = (stale / ".lock").open("w")
        except OSError:
            continue  # pruned by another worker mid-glob
        with lock_file:
            try:
                fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                continue  # still in use by another run
            # ignore_errors: a concurrent pruner may delete pieces under us
            shutil.rmtree(stale, ignore_errors=True)


async def _run_esphome_compile(
    config_path: Path, cwd: Path, env: dict[str, str]
) -> None:
    """Run `esphome compile`, retrying up to 3 times on a segfault."""
    max_retries = 3
    for attempt in range(max_retries):
        # Compile using subprocess, inheriting stdout/stderr to show progress
        proc = await asyncio.create_subprocess_exec(
            sys.executable,
            "-m",
            "esphome",
            "compile",
            str(config_path),
            cwd=cwd,
            stdout=None,  # Inherit stdout
            stderr=None,  # Inherit stderr
            stdin=asyncio.subprocess.DEVNULL,
            # Start in a new process group to isolate signal handling
            start_new_session=True,
            env=env,
            close_fds=False,
        )
        await proc.wait()

        if proc.returncode == 0:
            break
        if proc.returncode == -11 and attempt < max_retries - 1:
            # Segfault (-11 = SIGSEGV), retry
            print(
                f"Compilation segfaulted (attempt {attempt + 1}/{max_retries}), retrying..."
            )
            await asyncio.sleep(1)  # Brief pause before retry
            continue
        raise RuntimeError(
            f"Failed to compile {config_path}, return code: {proc.returncode}. "
            f"Run with 'pytest -s' to see compilation output."
        )


def _resolve_compiled_binary(config_path: Path) -> Path:
    """Load the config to learn the compiled ELF path (blocking, run in executor)."""
    CORE.reset()  # Reset CORE state between test runs
    CORE.config_path = config_path
    config = esphome.config.read_config(
        {"command": "compile", "config": str(config_path)}
    )
    if config is None:
        raise RuntimeError(f"Failed to read config from {config_path}")
    idedata = get_idedata(config)
    binary_path = Path(idedata.firmware_elf_path)
    if not binary_path.exists():
        raise RuntimeError(f"Compiled binary not found at {binary_path}")
    return binary_path


@pytest_asyncio.fixture
async def compile_esphome(
    integration_test_dir: Path,
    shared_platformio_cache: Path,
    request: pytest.FixtureRequest,
) -> AsyncGenerator[CompileFunction]:
    """Compile an ESPHome configuration and return the binary path."""

    async def _compile(config_path: Path) -> Path:
        # Use the shared PlatformIO cache for faster compilation
        # This avoids re-downloading dependencies for each test
        env = _get_platformio_env(shared_platformio_cache)
        loop = asyncio.get_running_loop()

        name = _shared_yaml_name(request)
        if name is None:
            await _run_esphome_compile(config_path, integration_test_dir, env)
            return await loop.run_in_executor(
                None, _resolve_compiled_binary, config_path
            )

        # Shared fixture: build in a hash-keyed dir so tests sharing a config
        # pay one full compile and later only a main.cpp (port) rebuild + relink
        shared_dir = (
            SHARED_BUILDS_ROOT / f"{name}-{_REPO_KEY}-{_shared_build_key(name)}"
        )
        shared_dir.mkdir(parents=True, exist_ok=True)
        await loop.run_in_executor(None, _prune_stale_builds, name, shared_dir)
        shared_config = shared_dir / f"{name}.yaml"
        private_binary = integration_test_dir / f"{name}.elf"
        content = await loop.run_in_executor(None, config_path.read_text)
        if content != getattr(request.node, "_shared_yaml_content", None):
            # The dir is keyed by the fixture source; a mutated config would be
            # cached under a hash that does not describe it
            raise RuntimeError(
                "shared_yaml tests must compile the yaml_config content unmodified"
            )
        # flock serializes concurrent xdist workers; closing the fd releases it.
        # Non-blocking retries keep the wait cancellable; a blocking LOCK_EX in
        # an executor thread would survive test cancellation holding the fd
        with (shared_dir / ".lock").open("w") as lock_file:
            for _ in range(_SHARED_LOCK_TIMEOUT_S * 10):
                try:
                    fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except BlockingIOError:
                    await asyncio.sleep(0.1)
            else:
                raise RuntimeError(f"Timed out waiting for the {shared_dir} lock")
            await loop.run_in_executor(None, shared_config.write_text, content)
            await _run_esphome_compile(shared_config, shared_dir, env)
            if (built := _shared_elf_paths.get(shared_dir)) is None:
                built = await loop.run_in_executor(
                    None, _resolve_compiled_binary, shared_config
                )
                _shared_elf_paths[shared_dir] = built
            # Copy out before unlocking: another worker may relink firmware.elf
            # while this test is still running its private copy
            await loop.run_in_executor(None, shutil.copy2, built, private_binary)
        return private_binary

    yield _compile


@asynccontextmanager
async def create_api_client(
    address: str = LOCALHOST,
    port: int = DEFAULT_API_PORT,
    password: str = "",
    noise_psk: str | None = None,
    client_info: str = "integration-test",
) -> AsyncGenerator[APIClient]:
    """Create an API client context manager."""
    client = APIClient(
        address=address,
        port=port,
        password=password,
        noise_psk=noise_psk,
        client_info=client_info,
    )
    try:
        yield client
    finally:
        await client.disconnect()


@pytest_asyncio.fixture
async def api_client_factory(
    unused_tcp_port: int,
) -> AsyncGenerator[APIClientFactory]:
    """Factory for creating API client context managers."""

    def _create_client(
        address: str = LOCALHOST,
        port: int | None = None,
        password: str = "",
        noise_psk: str | None = None,
        client_info: str = "integration-test",
    ) -> AbstractAsyncContextManager[APIClient]:
        return create_api_client(
            address=address,
            port=port if port is not None else unused_tcp_port,
            password=password,
            noise_psk=noise_psk,
            client_info=client_info,
        )

    yield _create_client


@asynccontextmanager
async def wait_and_connect_api_client(
    address: str = LOCALHOST,
    port: int = DEFAULT_API_PORT,
    password: str = "",
    noise_psk: str | None = None,
    client_info: str = "integration-test",
    timeout: float = API_CONNECTION_TIMEOUT,
    return_disconnect_event: bool = False,
) -> AsyncGenerator[APIClient | tuple[APIClient, asyncio.Event]]:
    """Wait for API to be available and connect."""
    client = APIClient(
        address=address,
        port=port,
        password=password,
        noise_psk=noise_psk,
        client_info=client_info,
    )

    # Create a future to signal when connected
    loop = asyncio.get_running_loop()
    connected_future: asyncio.Future[None] = loop.create_future()
    disconnect_event = asyncio.Event()

    async def on_connect() -> None:
        """Called when successfully connected."""
        disconnect_event.clear()  # Clear the disconnect event on new connection
        if not connected_future.done():
            connected_future.set_result(None)

    async def on_disconnect(expected_disconnect: bool) -> None:
        """Called when disconnected."""
        disconnect_event.set()
        if not connected_future.done() and not expected_disconnect:
            connected_future.set_exception(
                APIConnectionError("Disconnected before fully connected")
            )

    async def on_connect_error(err: Exception) -> None:
        """Called when connection fails."""
        if not connected_future.done():
            connected_future.set_exception(err)

    # Create and start the reconnect logic
    reconnect_logic = ReconnectLogic(
        client=client,
        on_connect=on_connect,
        on_disconnect=on_disconnect,
        zeroconf_instance=None,  # Not using zeroconf for integration tests
        name=f"{address}:{port}",
        on_connect_error=on_connect_error,
    )

    try:
        # Start the connection
        await reconnect_logic.start()

        # Wait for connection with timeout
        try:
            await asyncio.wait_for(connected_future, timeout=timeout)
        except TimeoutError as err:
            raise TimeoutError(
                f"Failed to connect to API after {timeout} seconds"
            ) from err

        if return_disconnect_event:
            yield client, disconnect_event
        else:
            yield client
    finally:
        # Stop reconnect logic and disconnect
        await reconnect_logic.stop()
        await client.disconnect()


@pytest_asyncio.fixture
async def api_client_connected(
    unused_tcp_port: int,
) -> AsyncGenerator[APIClientConnectedFactory]:
    """Factory for creating connected API client context managers."""

    def _connect_client(
        address: str = LOCALHOST,
        port: int | None = None,
        password: str = "",
        noise_psk: str | None = None,
        client_info: str = "integration-test",
        timeout: float = API_CONNECTION_TIMEOUT,
    ) -> AbstractAsyncContextManager[APIClient]:
        return wait_and_connect_api_client(
            address=address,
            port=port if port is not None else unused_tcp_port,
            password=password,
            noise_psk=noise_psk,
            client_info=client_info,
            timeout=timeout,
        )

    yield _connect_client


@pytest_asyncio.fixture
async def api_client_connected_with_disconnect(
    unused_tcp_port: int,
) -> AsyncGenerator:
    """Factory for creating connected API client context managers with disconnect event."""

    def _connect_client_with_disconnect(
        address: str = LOCALHOST,
        port: int | None = None,
        password: str = "",
        noise_psk: str | None = None,
        client_info: str = "integration-test",
        timeout: float = API_CONNECTION_TIMEOUT,
    ):
        return wait_and_connect_api_client(
            address=address,
            port=port if port is not None else unused_tcp_port,
            password=password,
            noise_psk=noise_psk,
            client_info=client_info,
            timeout=timeout,
            return_disconnect_event=True,
        )

    yield _connect_client_with_disconnect


async def _read_stream_lines(
    stream: asyncio.StreamReader,
    lines: list[str],
    output_stream: TextIO,
    line_callback: Callable[[str], None] | None = None,
) -> None:
    """Read lines from a stream, append to list, and echo to output stream."""
    log_parser = LogParser()
    while line := await stream.readline():
        decoded_line = (
            line.replace(b"\r", b"")
            .replace(b"\n", b"")
            .decode("utf8", "backslashreplace")
        )
        lines.append(decoded_line.rstrip())
        # Echo to stdout/stderr in real-time
        # Print without newline to avoid double newlines
        print(
            log_parser.parse_line(decoded_line, timestamp=""),
            file=output_stream,
            flush=True,
        )
        # Call the callback if provided
        if line_callback:
            line_callback(decoded_line.rstrip())


@asynccontextmanager
async def run_binary(
    binary_path: Path,
    line_callback: Callable[[str], None] | None = None,
) -> AsyncGenerator[tuple[asyncio.subprocess.Process, list[str]]]:
    """Run a binary under a PTY, capture log output, and clean up on exit.

    Yields the running ``Process`` and a live list of captured log lines.
    No port wait -- callers that need that should use
    ``run_binary_and_wait_for_port``."""
    # Create a pseudo-terminal to make the binary think it's running interactively
    # This is needed because the ESPHome host logger checks isatty()
    controller_fd, device_fd = pty.openpty()

    # Run the compiled binary with PTY
    process = await asyncio.create_subprocess_exec(
        str(binary_path),
        stdout=device_fd,
        stderr=device_fd,
        stdin=asyncio.subprocess.DEVNULL,
        # Start in a new process group to isolate signal handling
        start_new_session=True,
        pass_fds=(device_fd,),
        close_fds=False,
    )

    # Close the device end in the parent process
    os.close(device_fd)

    # Convert controller_fd to async streams for reading
    loop = asyncio.get_running_loop()
    controller_reader = asyncio.StreamReader()
    controller_protocol = asyncio.StreamReaderProtocol(controller_reader)
    controller_transport, _ = await loop.connect_read_pipe(
        lambda: controller_protocol, os.fdopen(controller_fd, "rb", 0)
    )

    if process.returncode is not None:
        raise RuntimeError(
            f"Process died immediately with return code {process.returncode}. "
            "Ensure the binary is valid and can run successfully."
        )

    stdout_lines: list[str] = []
    output_task = asyncio.create_task(
        _read_stream_lines(controller_reader, stdout_lines, sys.stdout, line_callback)
    )

    try:
        # Small yield to ensure the process has a chance to start
        await asyncio.sleep(0)
        yield process, stdout_lines
    finally:
        output_task.cancel()
        result = await asyncio.gather(output_task, return_exceptions=True)
        if isinstance(result[0], Exception) and not isinstance(
            result[0], asyncio.CancelledError
        ):
            print(f"Error reading from PTY: {result[0]}", file=sys.stderr)

        # Close the PTY transport (Unix only)
        if controller_transport is not None:
            controller_transport.close()

        # Cleanup: terminate the process gracefully
        if process.returncode is None:
            # Send SIGINT (Ctrl+C) for graceful shutdown
            process.send_signal(signal.SIGINT)
            try:
                await asyncio.wait_for(process.wait(), timeout=SIGINT_TIMEOUT)
            except TimeoutError:
                # If SIGINT didn't work, try SIGTERM
                process.terminate()
                try:
                    await asyncio.wait_for(process.wait(), timeout=SIGTERM_TIMEOUT)
                except TimeoutError:
                    # Last resort: SIGKILL
                    process.kill()
                    await process.wait()


@asynccontextmanager
async def run_binary_and_wait_for_port(
    binary_path: Path,
    host: str,
    port: int,
    timeout: float = PORT_WAIT_TIMEOUT,
    line_callback: Callable[[str], None] | None = None,
) -> AsyncGenerator[None]:
    """Run a binary, wait for it to open a port, and clean up on exit."""
    async with run_binary(binary_path, line_callback=line_callback) as (
        process,
        stdout_lines,
    ):
        loop = asyncio.get_running_loop()
        start_time = loop.time()
        while loop.time() - start_time < timeout:
            try:
                # Try to connect to the port
                _, writer = await asyncio.open_connection(host, port)
                writer.close()
                await writer.wait_closed()
                # Port is open, yield control
                yield
                return
            except (ConnectionRefusedError, OSError):
                # Check if process died
                if process.returncode is not None:
                    break
                # Port not open yet, wait a bit and try again
                await asyncio.sleep(PORT_POLL_INTERVAL)

        # Timeout or process died - build error message
        error_msg = f"Port {port} on {host} did not open within {timeout} seconds"

        if process.returncode is not None:
            error_msg += f"\nProcess exited with code: {process.returncode}"

        # Include any output collected so far
        if stdout_lines:
            error_msg += "\n\n--- Process Output ---\n"
            error_msg += "\n".join(stdout_lines[-100:])  # Last 100 lines

        raise TimeoutError(error_msg)


@asynccontextmanager
async def run_compiled_context(
    yaml_content: str,
    filename: str | None,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    port: int,
    port_socket: socket.socket | None = None,
    line_callback: Callable[[str], None] | None = None,
) -> AsyncGenerator[None]:
    """Context manager to write, compile and run an ESPHome configuration."""
    # Write the YAML config
    config_path = await write_yaml_config(yaml_content, filename)

    # Compile the configuration and get binary path
    binary_path = await compile_esphome(config_path)

    # Close the port socket right before running to release the port
    if port_socket is not None:
        port_socket.close()

    # Run the binary and wait for the API server to start
    async with run_binary_and_wait_for_port(
        binary_path, LOCALHOST, port, line_callback=line_callback
    ):
        yield


@pytest_asyncio.fixture
async def run_compiled(
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> AsyncGenerator[RunCompiledFunction]:
    """Write, compile and run an ESPHome configuration."""
    port, port_socket = reserved_tcp_port

    def _run_compiled(
        yaml_content: str,
        filename: str | None = None,
        line_callback: Callable[[str], None] | None = None,
    ) -> AbstractAsyncContextManager[asyncio.subprocess.Process]:
        return run_compiled_context(
            yaml_content,
            filename,
            write_yaml_config,
            compile_esphome,
            port,
            port_socket,
            line_callback=line_callback,
        )

    yield _run_compiled

"""Common fixtures for integration tests."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncGenerator, Generator
from contextlib import AbstractAsyncContextManager, asynccontextmanager
import logging
from pathlib import Path
import platform
import signal
import socket
import tempfile

from aioesphomeapi import APIClient, APIConnectionError, ReconnectLogic
import pytest
import pytest_asyncio

# Skip all integration tests on Windows
if platform.system() == "Windows":
    pytest.skip(
        "Integration tests are not supported on Windows", allow_module_level=True
    )

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
    RunFunction,
)


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
    # Get the test function name
    test_name: str = request.node.name
    # Extract the base test name (remove test_ prefix and any parametrization)
    base_name = test_name.replace("test_", "").partition("[")[0]

    # Load the fixture file
    fixture_path = Path(__file__).parent / "fixtures" / f"{base_name}.yaml"
    if not fixture_path.exists():
        raise FileNotFoundError(f"Fixture file not found: {fixture_path}")

    loop = asyncio.get_running_loop()
    content = await loop.run_in_executor(None, fixture_path.read_text)

    # Replace the port in the config if it contains api section
    if "api:" in content:
        # Add port configuration after api:
        content = content.replace("api:", f"api:\n  port: {unused_tcp_port}")

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


async def _run_esphome_command(
    command: str,
    config_path: Path,
    cwd: Path,
) -> asyncio.subprocess.Process:
    """Run an ESPHome command with the given arguments."""
    return await asyncio.create_subprocess_exec(
        "esphome",
        command,
        str(config_path),
        cwd=cwd,
        stdout=None,  # Inherit stdout
        stderr=None,  # Inherit stderr
        stdin=asyncio.subprocess.DEVNULL,
        # Start in a new process group to isolate signal handling
        start_new_session=True,
    )


@pytest_asyncio.fixture
async def compile_esphome(
    integration_test_dir: Path,
) -> AsyncGenerator[CompileFunction]:
    """Compile an ESPHome configuration."""

    async def _compile(config_path: Path) -> None:
        proc = await _run_esphome_command("compile", config_path, integration_test_dir)
        await proc.wait()
        if proc.returncode != 0:
            raise RuntimeError(
                f"Failed to compile {config_path}, return code: {proc.returncode}. "
                f"Run with 'pytest -s' to see compilation output."
            )

    yield _compile


@pytest_asyncio.fixture
async def run_esphome_process(
    integration_test_dir: Path,
) -> AsyncGenerator[RunFunction]:
    """Run an ESPHome process and manage its lifecycle."""
    processes: list[asyncio.subprocess.Process] = []

    async def _run(config_path: Path) -> asyncio.subprocess.Process:
        process = await _run_esphome_command("run", config_path, integration_test_dir)
        processes.append(process)
        return process

    yield _run

    # Cleanup: terminate all "run" processes gracefully
    for process in processes:
        if process.returncode is None:
            # Send SIGINT (Ctrl+C) for graceful shutdown of the running ESPHome instance
            process.send_signal(signal.SIGINT)
            try:
                await asyncio.wait_for(process.wait(), timeout=SIGINT_TIMEOUT)
            except asyncio.TimeoutError:
                # If SIGINT didn't work, try SIGTERM
                process.terminate()
                try:
                    await asyncio.wait_for(process.wait(), timeout=SIGTERM_TIMEOUT)
                except asyncio.TimeoutError:
                    # Last resort: SIGKILL
                    process.kill()
                    await process.wait()


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
) -> AsyncGenerator[APIClient]:
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

    async def on_connect() -> None:
        """Called when successfully connected."""
        if not connected_future.done():
            connected_future.set_result(None)

    async def on_disconnect(expected_disconnect: bool) -> None:
        """Called when disconnected."""
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
        except asyncio.TimeoutError:
            raise TimeoutError(f"Failed to connect to API after {timeout} seconds")

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


async def wait_for_port_open(
    host: str, port: int, timeout: float = PORT_WAIT_TIMEOUT
) -> None:
    """Wait for a TCP port to be open and accepting connections."""
    loop = asyncio.get_running_loop()
    start_time = loop.time()

    # Small yield to ensure the process has a chance to start
    await asyncio.sleep(0)

    while loop.time() - start_time < timeout:
        try:
            # Try to connect to the port
            _, writer = await asyncio.open_connection(host, port)
            writer.close()
            await writer.wait_closed()
            return  # Port is open
        except (ConnectionRefusedError, OSError):
            # Port not open yet, wait a bit and try again
            await asyncio.sleep(PORT_POLL_INTERVAL)

    raise TimeoutError(f"Port {port} on {host} did not open within {timeout} seconds")


@asynccontextmanager
async def run_compiled_context(
    yaml_content: str,
    filename: str | None,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    run_esphome_process: RunFunction,
    port: int,
    port_socket: socket.socket | None = None,
) -> AsyncGenerator[asyncio.subprocess.Process]:
    """Context manager to write, compile and run an ESPHome configuration."""
    # Write the YAML config
    config_path = await write_yaml_config(yaml_content, filename)

    # Compile the configuration
    await compile_esphome(config_path)

    # Close the port socket right before running to release the port
    if port_socket is not None:
        port_socket.close()

    # Run the ESPHome device
    process = await run_esphome_process(config_path)
    assert process.returncode is None, "Process died immediately"

    # Wait for the API server to start listening
    await wait_for_port_open(LOCALHOST, port, timeout=PORT_WAIT_TIMEOUT)

    try:
        yield process
    finally:
        # Process cleanup is handled by run_esphome_process fixture
        pass


@pytest_asyncio.fixture
async def run_compiled(
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    run_esphome_process: RunFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> AsyncGenerator[RunCompiledFunction]:
    """Write, compile and run an ESPHome configuration."""
    port, port_socket = reserved_tcp_port

    def _run_compiled(
        yaml_content: str, filename: str | None = None
    ) -> AbstractAsyncContextManager[asyncio.subprocess.Process]:
        return run_compiled_context(
            yaml_content,
            filename,
            write_yaml_config,
            compile_esphome,
            run_esphome_process,
            port,
            port_socket,
        )

    yield _run_compiled

"""Integration test for LD2420 component with mock UART.

Tests:
test_uart_mock_ld2420 (energy mode):
  1. Happy path - valid energy frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated energy frame - triggers "Energy frame too short" warning (PR #14458 bug #3)
  4. Buffer overflow recovery - overflow resets the parser
  5. Post-overflow parsing - next valid frame after overflow is parsed correctly
  6. TX logging - verifies LD2420 sends expected setup commands

test_uart_mock_ld2420_simple (simple mode):
  1. Happy path - valid simple mode text frame publishes correct values
  2. Garbage resilience
  3. Buffer overflow recovery
  4. 16-digit distance triggers infinite loop pre-fix (PR #14458 bug #1)
  5. Post-bug-trigger recovery proves the parser survived

test_uart_mock_ld2420_warm_restart (module streaming at boot):
  Simulates a warm restart where the module stayed powered and streams energy
  frames from the moment the firmware starts. Asserts the component never
  transmits before receiving data from the module, completes setup against
  the live stream, and publishes sensor data.

test_uart_mock_ld2420_delayed_boot (module boots slower than the ESP):
  Simulates a cold boot where the module is silent for 2 seconds. The module
  locks up until power cycled if it receives data before sending its first
  frame, so the component must stay quiet until the module talks, then
  complete setup and keep parsing the stream.

test_uart_mock_ld2420_restart_button (module restart action):
  Presses the restart button after setup. The restart hits the module mid
  transmission, so a few tail bytes of the in-flight frame arrive right after
  the restart command, then the module is silent for 2 seconds while it
  boots. The component must not treat the tail bytes as proof the module is
  up and must only re-run its handshake after the module's first post-boot
  frame; transmitting into the boot window locks up real hardware.
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_ld2420(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2420 energy mode: happy path, truncated frame, overflow, and recovery."""
    # Replace external component path placeholder
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track overflow warning in logs
    overflow_seen = loop.create_future()

    # Track "Energy frame too short" warning (PR #14458 bug #3 fix)
    # This message ONLY exists after the fix. Pre-fix, handle_energy_mode_
    # silently reads past the buffer without any warning.
    truncated_frame_warning_seen = loop.create_future()

    # Track TX data logged by the mock for assertions
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Max command length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)
        if "Energy frame too short" in line and not truncated_frame_warning_seen.done():
            truncated_frame_warning_seen.set_result(True)
        # Capture all TX log lines from uart_mock
        if "uart_mock" in line and "TX " in line:
            tx_log_lines.append(line)

    collector = SensorStateCollector(
        sensor_names=["moving_distance"],
        binary_sensor_names=["has_target"],
    )

    # Signal when we see recovery frame values
    recovery_received = collector.add_waiter(
        lambda: pytest.approx(50.0) in collector.sensor_states["moving_distance"]
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        # Set up initial state helper
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 values: moving=100, has_target=true
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.binary_states["has_target"][0] is True

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated + overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Verify truncated frame warning was logged (PR #14458 bug #3)
        # This assertion FAILS before PR #14458 because the length check
        # and warning message did not exist.
        assert truncated_frame_warning_seen.done(), (
            "Expected 'Energy frame too short' warning in logs. "
            "This indicates PR #14458 fix for handle_energy_mode_ length "
            "validation is missing."
        )

        # Verify LD2420 sent setup commands (TX logging)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        # Verify command frame header appears (FD:FC:FB:FA)
        assert "FD.FC.FB.FA" in tx_data or "FD:FC:FB:FA" in tx_data, (
            "Expected LD2420 command frame header FD:FC:FB:FA in TX log"
        )
        # Verify command frame footer appears (04:03:02:01)
        assert "04.03.02.01" in tx_data or "04:03:02:01" in tx_data, (
            "Expected LD2420 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame values (Phase 5, after overflow)
        recovery_values = [
            v
            for v in collector.sensor_states["moving_distance"]
            if v == pytest.approx(50.0)
        ]
        assert len(recovery_values) >= 1, (
            f"Expected moving_distance=50 in recovery, got: {collector.sensor_states['moving_distance']}"
        )


async def _run_listen_first_test(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    *,
    post_setup_distance: float | None = None,
) -> None:
    """Shared body for the listen-first startup tests.

    Asserts the component never transmits before the module has sent data
    (real hardware locks up until power cycled if it does), that the setup
    handshake completes, and that sensor data publishes. When
    post_setup_distance is given, additionally waits for that value to prove
    streaming still works after the handshake.
    """
    loop = asyncio.get_running_loop()

    setup_complete = loop.create_future()
    rx_seen = False
    tx_before_rx = False
    failure_lines: list[str] = []

    def line_callback(line: str) -> None:
        nonlocal rx_seen, tx_before_rx
        if "uart_mock" in line:
            if "RX inject" in line or "Injecting" in line:
                rx_seen = True
            elif "TX " in line and not rx_seen:
                tx_before_rx = True
        if (
            "Module setup complete; firmware v2.0.0" in line
            and not setup_complete.done()
        ):
            setup_complete.set_result(True)
        if (
            "marked FAILED" in line
            or "Communication failed" in line
            or "No data received from the module" in line
        ):
            failure_lines.append(line)

    collector = SensorStateCollector(
        sensor_names=["moving_distance"],
        binary_sensor_names=["has_target"],
    )

    post_setup_received = None
    if post_setup_distance is not None:
        post_setup_received = collector.add_waiter(
            lambda: (
                pytest.approx(post_setup_distance)
                in collector.sensor_states["moving_distance"]
            )
        )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Setup handshake must complete once the module has talked
        try:
            await asyncio.wait_for(setup_complete, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for 'Module setup complete' log line. "
                "The startup state machine did not finish its handshake."
            )

        # Sensor data must flow from the stream
        try:
            await collector.wait_for_all(timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for sensor data. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.binary_states["has_target"][0] is True

        if post_setup_received is not None:
            try:
                await asyncio.wait_for(post_setup_received, timeout=5.0)
            except TimeoutError:
                pytest.fail(
                    f"Timeout waiting for post-setup frame "
                    f"(distance={post_setup_distance}). Received:\n"
                    f"  moving_distance: {collector.sensor_states['moving_distance']}"
                )

        # The component must never transmit before the module has talked;
        # real hardware locks up until power cycled if it does.
        assert not tx_before_rx, (
            "Component transmitted on the UART before receiving any data "
            "from the module; this locks up real LD2420 hardware"
        )

        assert not failure_lines, f"Unexpected failure log lines: {failure_lines}"


@pytest.mark.asyncio
async def test_uart_mock_ld2420_warm_restart(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Module streams from boot; component must listen first, then set up."""
    await _run_listen_first_test(yaml_config, run_compiled, api_client_connected)


@pytest.mark.asyncio
async def test_uart_mock_ld2420_delayed_boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Module silent for 2 s; component must not transmit into the boot window."""
    await _run_listen_first_test(
        yaml_config, run_compiled, api_client_connected, post_setup_distance=50.0
    )


@pytest.mark.asyncio
async def test_uart_mock_ld2420_restart_button(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Restart action must not transmit into the module's boot window."""
    loop = asyncio.get_running_loop()

    setup_complete_count = 0
    first_setup_complete = loop.create_future()
    second_setup_complete = loop.create_future()
    restart_seen = False
    module_frame_after_restart_seen = False
    tx_into_boot_window = False
    failure_lines: list[str] = []

    def line_callback(line: str) -> None:
        nonlocal setup_complete_count, restart_seen, module_frame_after_restart_seen
        nonlocal tx_into_boot_window
        if "Module setup complete; firmware v2.0.0" in line:
            setup_complete_count += 1
            if setup_complete_count == 1 and not first_setup_complete.done():
                first_setup_complete.set_result(True)
            elif setup_complete_count == 2 and not second_setup_complete.done():
                second_setup_complete.set_result(True)
        if "[ld2420" in line and "Restarting" in line:
            restart_seen = True
        if restart_seen and "RX inject 45 bytes" in line:
            # The module's first frame after its simulated 2 s boot
            module_frame_after_restart_seen = True
        if (
            restart_seen
            and not module_frame_after_restart_seen
            and "uart_mock" in line
            and "TX " in line
            and "FF:00:02:00" in line
        ):
            # Config mode enable transmitted before the module's first
            # post-boot frame; on real hardware this locks the module up
            tx_into_boot_window = True
        if "marked FAILED" in line or "Communication failed" in line:
            failure_lines.append(line)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(lambda s: None))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Wait for the initial startup handshake to finish
        try:
            await asyncio.wait_for(first_setup_complete, timeout=10.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for the initial 'Module setup complete'")

        # Restart the module; the button automation also injects the in-flight
        # frame tail immediately and the module's first frame 2 s later
        restart_btn = find_entity(entities, "restart_module", ButtonInfo)
        assert restart_btn is not None, "Restart Module button not found"
        client.button_command(restart_btn.key)

        # The handshake must complete again after the module comes back
        try:
            await asyncio.wait_for(second_setup_complete, timeout=15.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for 'Module setup complete' after the restart. "
                "The component did not recover from the module restart."
            )

        assert not tx_into_boot_window, (
            "Component transmitted the config handshake into the module's "
            "boot window after a restart; the in-flight frame tail bytes must "
            "not count as proof the module is up"
        )

        assert not failure_lines, f"Unexpected failure log lines: {failure_lines}"


@pytest.mark.asyncio
async def test_uart_mock_ld2420_simple(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2420 simple mode: happy path, overflow, and 16-digit bug trigger."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track overflow warning in logs
    overflow_seen = loop.create_future()

    def line_callback(line: str) -> None:
        if "Max command length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)

    collector = SensorStateCollector(
        sensor_names=["moving_distance"],
        binary_sensor_names=["has_target"],
    )

    # Signal for recovery frames
    recovery_received = collector.add_waiter(
        lambda: pytest.approx(50.0) in collector.sensor_states["moving_distance"]
    )
    post_bug_received = collector.add_waiter(
        lambda: pytest.approx(25.0) in collector.sensor_states["moving_distance"]
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1: simple mode "ON Range 0100\r\n" → distance=100, presence=true
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.binary_states["has_target"][0] is True

        # Wait for Phase 4 recovery (distance=50) after overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  moving_distance: {collector.sensor_states['moving_distance']}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Wait for Phase 6: distance=25 (post-16-digit-bug recovery)
        # This assertion FAILS before PR #14458 because the 16-digit frame
        # in Phase 5 causes an infinite loop in handle_simple_mode_ pre-fix.
        # The binary hangs, Phase 6 never fires, and this wait times out.
        try:
            await asyncio.wait_for(post_bug_received, timeout=8.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for post-bug recovery (distance=25). "
                f"This likely means Phase 5 (16-digit frame) caused an infinite "
                f"loop in handle_simple_mode_, indicating PR #14458 bug #1 fix "
                f"is missing.\n"
                f"  moving_distance values: {collector.sensor_states['moving_distance']}"
            )

        # Verify post-bug value
        post_bug_values = [
            v
            for v in collector.sensor_states["moving_distance"]
            if v == pytest.approx(25.0)
        ]
        assert len(post_bug_values) >= 1, (
            f"Expected moving_distance=25 after 16-digit test, "
            f"got: {collector.sensor_states['moving_distance']}"
        )

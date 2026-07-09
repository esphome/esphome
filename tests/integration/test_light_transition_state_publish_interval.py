"""Integration tests for transition_state_publish_interval behavior on lights.

These tests verify both legacy behavior (interval = 0s) and the new
interval-publishing semantics for transitions and flashes, using host mode.
"""

from __future__ import annotations

import asyncio
from pathlib import Path
from typing import Any

import aioesphomeapi
from aioesphomeapi import EntityState, LightState
import pytest
import pytest_asyncio

from .types import APIClientConnectedFactory, RunCompiledFunction


async def _collect_state_timeline(
    client: aioesphomeapi.APIClient,
    target_key: int,
    action: Any,
    duration: float,
) -> list[tuple[float, LightState]]:
    """Collect a timeline of LightState updates for a single entity key.

    Args:
        client: Connected API client.
        target_key: The entity key of the light to track.
        action: A callable or coroutine that triggers the transition.
        duration: How long to record states after invoking action.

    Returns:
        List of (timestamp, LightState) tuples.
    """
    loop = asyncio.get_running_loop()
    start_time = loop.time()
    end_time = start_time + duration
    timeline: list[tuple[float, LightState]] = []

    def on_state(state: EntityState) -> None:
        if isinstance(state, LightState) and state.key == target_key:
            now = loop.time()
            # Only record states within the requested collection window.
            if now <= end_time:
                timeline.append((now - start_time, state))

    client.subscribe_states(on_state)

    # Run action (sync or async)
    if asyncio.iscoroutinefunction(action):
        await action()
    else:
        action()

    # Collect for the requested duration
    await asyncio.sleep(duration)
    return timeline


@pytest_asyncio.fixture
async def yaml_config(
    request: pytest.FixtureRequest,
    unused_tcp_port: int,
) -> str:
    test_name: str = request.node.name
    base_name = test_name.replace("test_", "").partition("[")[0]

    alias_map = {
        "transition_interval_zero_behaves_like_legacy": "light_transition_state_publish_interval",
        "transition_interval_nonzero_emits_intermediate_updates": "light_transition_state_publish_interval",
        "flash_interval_emits_intermediate_updates": "light_transition_state_publish_interval",
    }
    base_name = alias_map.get(base_name, base_name)

    fixture_path = Path(__file__).parent / "fixtures" / f"{base_name}.yaml"
    if not fixture_path.exists():
        raise FileNotFoundError(f"Fixture file not found: {fixture_path}")

    loop = asyncio.get_running_loop()
    content = await loop.run_in_executor(None, fixture_path.read_text)

    if "api:" in content:
        content = content.replace("api:", f"api:\n  port: {unused_tcp_port}")

    if "esphome:" in content and "platformio_options:" not in content:
        content = content.replace(
            "esphome:",
            "esphome:\n"
            "  platformio_options:\n"
            "    build_flags:\n"
            '      - "-DDEBUG"\n'
            '      - "-g"',
        )

    return content


@pytest.mark.asyncio
async def test_transition_interval_zero_behaves_like_legacy(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """When interval == 0s, behavior should match legacy semantics.

    Expected behavior:
      - A transition with transition_length > 0 publishes only the final state
        (no intermediate publishes caused by the interval logic).
    """
    # Force interval to 0s in the fixture to emulate legacy behavior
    yaml_zero = yaml_config.replace(
        "transition_state_publish_interval: 0.2s",
        "transition_state_publish_interval: 0s",
    )

    async with run_compiled(yaml_zero), api_client_connected() as client:
        # Get entities and locate lights
        entities, _ = await client.list_entities_services()

        # Map object_id to LightInfo
        light_infos: dict[str, aioesphomeapi.LightInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.LightInfo)
        }

        mono = light_infos.get("test_mono_light")
        rgb = light_infos.get("test_rgb_light")
        cwww = light_infos.get("test_cwww_light")
        assert mono is not None
        assert rgb is not None
        assert cwww is not None

        # Helper to run a transition and collect states
        async def run_case(
            light: aioesphomeapi.LightInfo,
        ) -> list[tuple[float, LightState]]:
            return await _collect_state_timeline(
                client,
                light.key,
                lambda: client.light_command(
                    key=light.key,
                    state=True,
                    brightness=0.8,
                    transition_length=1.0,
                ),
                duration=1.5,
            )

        mono_timeline = await run_case(mono)
        rgb_timeline = await run_case(rgb)
        cwww_timeline = await run_case(cwww)

        for name, tl in (
            ("mono", mono_timeline),
            ("rgb", rgb_timeline),
            ("cwww", cwww_timeline),
        ):
            assert len(tl) >= 1, f"Expected at least one state update for {name}"
            # With interval 0, we expect effectively a single publish at or near the final state.
            # Allow a tiny number of extra states, but ensure they are not a dense stream.
            assert len(tl) <= 3, (
                f"Unexpectedly many publishes for {name} with interval=0s: {len(tl)}"
            )
            # Last state should be at target brightness
            _, last = tl[-1]
            assert last.brightness is not None
            assert last.brightness == pytest.approx(0.8)


@pytest.mark.asyncio
async def test_transition_interval_nonzero_emits_intermediate_updates(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """When interval > 0, transitions should emit multiple intermediate updates.

    We verify this on the monochromatic light:
      - First publish reflects the starting state (no jump directly to final).
      - Multiple updates occur during the 1s transition.
      - Final state matches the target brightness.
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        light_infos: dict[str, aioesphomeapi.LightInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.LightInfo)
        }
        mono = light_infos.get("test_mono_light")
        assert mono is not None

        # Ensure starting state is off / low brightness
        client.light_command(key=mono.key, state=False, brightness=0.0)
        await asyncio.sleep(0.2)

        mono_timeline = await _collect_state_timeline(
            client,
            mono.key,
            lambda: client.light_command(
                key=mono.key,
                state=True,
                brightness=1.0,
                transition_length=1.0,
            ),
            duration=1.5,
        )

        assert len(mono_timeline) >= 3, (
            "Expected multiple publishes during transition when interval > 0, "
            f"got {len(mono_timeline)}"
        )

        # Inspect last entry
        last_t, last_state = mono_timeline[-1]

        assert last_state.brightness is not None

        # Extract brightness time series (including initial and final publishes).
        brightness_values = [
            state.brightness
            for _, state in mono_timeline
            if state.brightness is not None
        ]
        assert brightness_values, "Expected brightness values in timeline"

        # We expect: brightness starts near 0, rises through several intermediate
        # values, and ends near 1.0. Allow some flexibility for host-mode timing
        # and a possible initial pre-transition sample.
        assert len(brightness_values) >= 5, (
            "Expected at least five brightness updates for interval=0.2s, "
            f"got {len(brightness_values)} values={brightness_values}"
        )

        values = list(brightness_values)
        # If the very first sample is much higher than the second (e.g. a
        # pre-transition state), drop it before analyzing the ramp.
        if len(values) >= 2 and values[0] > values[1] + 0.2:
            values = values[1:]

        min_brightness = min(values)
        max_brightness = max(values)

        # Ensure we genuinely ramp from near 0 up to near 1.
        assert min_brightness == pytest.approx(0.0, abs=0.1), (
            "Expected minimum brightness near 0.0 during transition; "
            f"got min={min_brightness}, values={values}"
        )
        assert max_brightness == pytest.approx(1.0, abs=0.05), (
            "Expected maximum brightness near 1.0 during transition; "
            f"got max={max_brightness}, values={values}"
        )

        # There should be multiple intermediate values strictly between 0 and 1.
        intermediate = [b for b in values if 0.1 < b < 0.9]
        assert len(intermediate) >= 2, (
            "Expected at least two intermediate brightness values between 0.1 and 0.9; "
            f"got values={values}"
        )

        # Brightness should be non-decreasing over time (within a small tolerance
        # to account for numeric jitter in host mode).
        for i in range(len(values) - 1):
            assert values[i + 1] >= values[i] - 0.1, (
                "Brightness should not drop significantly during transition; "
                f"values={values}"
            )

        # Rough timing sanity check: we expect states roughly every ~0.2s (configured interval),
        # so having more than 1 second of total span and at least a couple of intermediate points
        # is sufficient as a non-flaky check.
        assert last_t >= 0.8, f"Transition appeared to end too quickly: {last_t}s"


@pytest.mark.asyncio
async def test_light_transition_state_publish_interval(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Validate interval publishing for default, RGB, and CWWW transitions.

    This test uses the shared fixture "light_transition_state_publish_interval.yaml"
    (via the yaml_config fixture) which defines mono, RGB, and CWWW lights with
    default_transition_length: 1s and transition_state_publish_interval: 0.2s.

    It verifies that:
      - Default transitions (no explicit transition_length) use the configured
        default_transition_length while emitting intermediate updates.
      - RGB transitions emit multiple intermediate updates and reach the target
        brightness while changing color over time.
      - CWWW/CT transitions emit intermediate updates and reach the target
        color temperature.
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        light_infos: dict[str, aioesphomeapi.LightInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.LightInfo)
        }

        mono = light_infos.get("test_mono_light")
        rgb = light_infos.get("test_rgb_light")
        cwww = light_infos.get("test_cwww_light")
        assert mono is not None
        assert rgb is not None
        assert cwww is not None

        # Ensure a known starting state for all lights.
        for light in (mono, rgb, cwww):
            client.light_command(key=light.key, state=False, brightness=0.0)
        await asyncio.sleep(0.2)

        # 1) Default transition (no explicit transition_length) should use
        #    default_transition_length: 1s and emit multiple updates.
        mono_timeline_default = await _collect_state_timeline(
            client,
            mono.key,
            lambda: client.light_command(
                key=mono.key,
                state=True,
                brightness=1.0,
                # No transition_length: rely on default_transition_length.
            ),
            duration=1.5,
        )

        assert len(mono_timeline_default) >= 3, (
            "Expected multiple publishes during default transition when interval > 0, "
            f"got {len(mono_timeline_default)}"
        )
        last_t_default, last_state_default = mono_timeline_default[-1]
        assert last_state_default.brightness is not None
        assert last_state_default.brightness == pytest.approx(1.0, abs=0.05)
        # Expect the transition to span close to the configured default (1s).
        assert last_t_default >= 0.8, (
            f"Default transition appeared to end too quickly: {last_t_default}s"
        )

        # 2) RGB transition: verify multiple intermediate updates and final
        #    brightness while color components change over time.
        rgb_timeline = await _collect_state_timeline(
            client,
            rgb.key,
            lambda: client.light_command(
                key=rgb.key,
                state=True,
                brightness=1.0,
                rgb=(1.0, 0.0, 0.0),
                transition_length=1.0,
            ),
            duration=1.5,
        )

        assert len(rgb_timeline) >= 3, (
            "Expected multiple publishes during RGB transition when interval > 0, "
            f"got {len(rgb_timeline)}"
        )

        rgb_times, rgb_states = zip(*rgb_timeline, strict=True)
        last_t_rgb = rgb_times[-1]
        last_state_rgb = rgb_states[-1]

        assert last_state_rgb.brightness is not None
        assert last_state_rgb.brightness == pytest.approx(1.0, abs=0.05)

        # We primarily care that the transition spans a reasonable amount of
        # time and produces multiple interval-driven publishes; the exact
        # per-channel waveform is implementation-dependent.
        assert last_t_rgb >= 0.8, (
            f"RGB transition appeared to end too quickly: {last_t_rgb}s"
        )

        # 3) CWWW/CT transition: verify intermediate updates and that the final
        #    color temperature reaches the requested value.
        cwww_timeline = await _collect_state_timeline(
            client,
            cwww.key,
            lambda: client.light_command(
                key=cwww.key,
                state=True,
                color_temperature=300.0,
                brightness=1.0,
                transition_length=1.0,
            ),
            duration=1.5,
        )

        assert len(cwww_timeline) >= 3, (
            "Expected multiple publishes during CWWW/CT transition when interval > 0, "
            f"got {len(cwww_timeline)}"
        )

        cwww_times, cwww_states = zip(*cwww_timeline, strict=True)
        last_t_cwww = cwww_times[-1]
        last_state_cwww = cwww_states[-1]

        ct_values = [
            state.color_temperature
            for state in cwww_states
            if state.color_temperature is not None
        ]
        assert ct_values, "Expected color_temperature values in CWWW timeline"

        min_ct = min(ct_values)
        max_ct = max(ct_values)
        # We expect the final color temperature near the requested value and at
        # least one intermediate value that differs from it.
        assert last_state_cwww.color_temperature is not None
        assert last_state_cwww.color_temperature == pytest.approx(300.0, abs=5.0)
        assert min_ct < max_ct, (
            "Expected at least one intermediate color_temperature value different "
            f"from the final; values={ct_values}"
        )
        assert last_t_cwww >= 0.8, (
            f"CWWW/CT transition appeared to end too quickly: {last_t_cwww}s"
        )


@pytest.mark.asyncio
async def test_transition_interval_persistence_semantics(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    save_log_count = 0

    def on_log_line(line: str) -> None:
        nonlocal save_log_count
        if "LightState preferences saved:" in line:
            save_log_count += 1

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        light_infos: dict[str, aioesphomeapi.LightInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.LightInfo)
        }
        button_infos: dict[str, aioesphomeapi.ButtonInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.ButtonInfo)
        }

        mono = light_infos.get("test_mono_light_persist")
        run_button = button_infos.get("run_persistence_transition")
        assert mono is not None
        assert run_button is not None

        client.light_command(key=mono.key, state=False, brightness=0.0)
        await asyncio.sleep(0.2)

        first_run_timeline = await _collect_state_timeline(
            client,
            mono.key,
            lambda: client.button_command(run_button.key),
            duration=1.5,
        )

        assert first_run_timeline

        times, states = zip(*first_run_timeline, strict=True)
        last_t = times[-1]
        last_state = states[-1]

        brightness_values = [s.brightness for s in states if s.brightness is not None]
        assert brightness_values

        min_brightness = min(brightness_values)
        assert min_brightness < 0.95

        assert last_state.brightness is not None
        assert last_state.brightness == pytest.approx(1.0, abs=0.05)
        assert last_t >= 0.8

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        light_infos: dict[str, aioesphomeapi.LightInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.LightInfo)
        }
        mono = light_infos.get("test_mono_light_persist")
        assert mono is not None

        loop = asyncio.get_running_loop()
        initial_state_future: asyncio.Future[LightState] = loop.create_future()

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, LightState)
                and state.key == mono.key
                and not initial_state_future.done()
            ):
                initial_state_future.set_result(state)

        client.subscribe_states(on_state)

        try:
            initial_state = await asyncio.wait_for(initial_state_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Did not receive initial light state after restart")

        # After restart, we expect the restored brightness (if reported) to
        # match the final value saved at the end of the transition.
        if initial_state.brightness is not None:
            assert initial_state.brightness == pytest.approx(1.0, abs=0.05)

    # Across both runs we expect exactly one preference save for this light
    # during the save=true interval transition.
    assert save_log_count == 1


@pytest.mark.asyncio
async def test_flash_interval_emits_intermediate_updates(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        light_infos: dict[str, aioesphomeapi.LightInfo] = {
            e.object_id: e for e in entities if isinstance(e, aioesphomeapi.LightInfo)
        }

        mono = light_infos.get("test_mono_light")
        assert mono is not None

        client.light_command(key=mono.key, state=True, brightness=0.4)
        await asyncio.sleep(0.2)

        baseline_timeline = await _collect_state_timeline(
            client,
            mono.key,
            lambda: None,
            duration=0.2,
        )

        assert baseline_timeline
        _, baseline_state = baseline_timeline[-1]
        assert baseline_state.brightness is not None
        baseline_brightness = baseline_state.brightness

        flash_timeline = await _collect_state_timeline(
            client,
            mono.key,
            lambda: client.light_command(
                key=mono.key,
                flash_length=1.0,
            ),
            duration=1.5,
        )

        assert len(flash_timeline) >= 3

        times, states = zip(*flash_timeline, strict=True)
        last_t = times[-1]
        last_state = states[-1]

        brightness_values = [s.brightness for s in states if s.brightness is not None]
        assert brightness_values

        assert last_state.brightness is not None
        assert last_state.brightness == pytest.approx(baseline_brightness, abs=0.15)
        assert last_t >= 0.8

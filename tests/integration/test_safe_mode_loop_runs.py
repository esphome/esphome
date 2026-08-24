"""Regression test for safe_mode + looping_components init ordering.

Reproduces the bug fixed in https://github.com/esphome/esphome/pull/16269:
``App.looping_components_.init(...)`` was emitted at ``CoroPriority.FINAL``,
which placed it *after* the ``safe_mode`` early-return in ``setup_app()``.
When safe mode was entered, the ``FixedVector`` backing the looping-component
list was never sized, ``looping_components_active_end_`` stayed at 0, and
``loop()`` iterated zero components -- so any looping component above
``CoroPriority.APPLICATION`` (e.g. wifi, logger) never ran.

The test forces safe mode by writing ``ENTER_SAFE_MODE_MAGIC`` to the host
preferences file before booting, then asserts that ``Logger::loop()`` runs
by logging from a non-main thread. Non-main-thread logs are buffered in
``TaskLogBuffer`` and only emitted to the console when ``Logger::loop()``
drains the buffer. Without the fix, the marker stays in the buffer
forever; with the fix, it reaches the console.

The API server (``CoroPriority.WEB``, 40) is registered below safe_mode
(``CoroPriority.APPLICATION``, 50), so it's never set up when safe mode
is active and ``run_compiled`` would hang waiting for the API port.
This test uses ``run_binary`` directly to skip the port wait.
"""

from __future__ import annotations

import asyncio
import re
import struct

import pytest

from .conftest import run_binary
from .host_prefs import clear_host_prefs, write_host_pref
from .types import CompileFunction, ConfigWriter

# Must match esphome::safe_mode::RTC_KEY in safe_mode.h
SAFE_MODE_RTC_KEY = 233825507
# Must match esphome::safe_mode::SafeModeComponent::ENTER_SAFE_MODE_MAGIC
ENTER_SAFE_MODE_MAGIC = 0x5AFE5AFE

DEVICE_NAME = "safe-mode-loop-runs"
THREAD_LOG_MARKER = "looping component ran in safe mode"


@pytest.mark.asyncio
async def test_safe_mode_loop_runs(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
) -> None:
    """When safe mode is active, ``App.loop()`` must still iterate looping
    components -- proven here by a thread-logged marker reaching the
    console (which requires ``Logger::loop()`` to run)."""
    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    # Compile finished successfully; pre-populate prefs so the *next* run
    # enters safe mode immediately.
    write_host_pref(
        DEVICE_NAME, SAFE_MODE_RTC_KEY, struct.pack("<I", ENTER_SAFE_MODE_MAGIC)
    )

    try:
        loop = asyncio.get_running_loop()
        safe_mode_active = loop.create_future()
        thread_log_seen = loop.create_future()
        safe_mode_pattern = re.compile(r"SAFE MODE IS ACTIVE")
        thread_log_pattern = re.compile(re.escape(THREAD_LOG_MARKER))

        def on_log(line: str) -> None:
            if not safe_mode_active.done() and safe_mode_pattern.search(line):
                safe_mode_active.set_result(True)
            if not thread_log_seen.done() and thread_log_pattern.search(line):
                thread_log_seen.set_result(True)

        async with run_binary(binary_path, line_callback=on_log):
            try:
                await asyncio.wait_for(safe_mode_active, timeout=15.0)
            except TimeoutError:
                pytest.fail(
                    "Did not observe 'SAFE MODE IS ACTIVE' -- safe mode "
                    "didn't trigger, so this test isn't exercising the bug."
                )
            try:
                await asyncio.wait_for(thread_log_seen, timeout=10.0)
            except TimeoutError:
                pytest.fail(
                    f"Did not observe thread-logged marker {THREAD_LOG_MARKER!r} "
                    "within timeout. Logger::loop() never drained the task "
                    "log buffer, meaning App.looping_components_ was never "
                    "sized -- this is the regression #16269 fixed."
                )
    finally:
        clear_host_prefs(DEVICE_NAME)

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

import pytest

from .conftest import run_binary
from .host_prefs import clear_host_prefs, force_safe_mode
from .log_utils import LineWaiter
from .types import CompileFunction, ConfigWriter
from .wait_utils import wait_or_fail

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
    force_safe_mode(DEVICE_NAME)

    try:
        waiter = LineWaiter()
        async with run_binary(binary_path, line_callback=waiter.callback):
            await wait_or_fail(
                waiter.wait_for("SAFE MODE IS ACTIVE", timeout=15.0),
                15.0,
                "Did not observe 'SAFE MODE IS ACTIVE' -- safe mode "
                "didn't trigger, so this test isn't exercising the bug.",
            )
            await wait_or_fail(
                waiter.wait_for(THREAD_LOG_MARKER),
                10.0,
                f"Did not observe thread-logged marker {THREAD_LOG_MARKER!r} "
                "within timeout. Logger::loop() never drained the task "
                "log buffer, meaning App.looping_components_ was never "
                "sized -- this is the regression #16269 fixed.",
            )
    finally:
        clear_host_prefs(DEVICE_NAME)

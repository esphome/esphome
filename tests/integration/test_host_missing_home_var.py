"""Test host preferences behavior when HOME environment variable is not set."""

from __future__ import annotations

import asyncio
import sys

import pytest

from .types import CompileFunction, ConfigWriter


@pytest.mark.asyncio
async def test_host_missing_home_var(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    unused_tcp_port: int,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test that the host platform exits gracefully when HOME is not set."""
    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    monkeypatch.delenv("HOME", raising=False)

    stdout_lines: list[str] = []
    stderr_lines: list[str] = []

    process = await asyncio.create_subprocess_exec(
        str(binary_path),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        stdin=asyncio.subprocess.DEVNULL,
        start_new_session=True,
        close_fds=False,
    )

    async def read_stdout():
        if process.stdout:
            async for line in process.stdout:
                decoded = line.decode("utf8", "backslashreplace").rstrip()
                stdout_lines.append(decoded)
                print(f"[STDOUT] {decoded}", file=sys.stdout, flush=True)

    async def read_stderr():
        if process.stderr:
            async for line in process.stderr:
                decoded = line.decode("utf8", "backslashreplace").rstrip()
                stderr_lines.append(decoded)
                print(f"[STDERR] {decoded}", file=sys.stderr, flush=True)

    read_tasks = [
        asyncio.create_task(read_stdout()),
        asyncio.create_task(read_stderr()),
    ]

    try:
        await asyncio.wait_for(process.wait(), timeout=5.0)
    except TimeoutError:
        pytest.fail("Process did not exit within timeout when HOME was unset")
    finally:
        for task in read_tasks:
            task.cancel()
        await asyncio.gather(*read_tasks, return_exceptions=True)

    assert process.returncode == 1, (
        f"Expected exit code 1, got {process.returncode}\n"
        f"STDOUT:\n" + "\n".join(stdout_lines) + "\n"
        "STDERR:\n" + "\n".join(stderr_lines)
    )

    all_output = "\n".join(stdout_lines + stderr_lines)
    expected_message = (
        "HOME environment variable not set, which is needed for saving preferences"
    )
    assert expected_message in all_output, (
        f"Expected warning message not found in output.\n"
        f"Expected: {expected_message}\n"
        f"STDOUT:\n" + "\n".join(stdout_lines) + "\n"
        "STDERR:\n" + "\n".join(stderr_lines)
    )

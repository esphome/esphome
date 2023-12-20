from __future__ import annotations

import asyncio
from collections.abc import Iterable


async def async_system_command_status(command: Iterable[str]) -> bool:
    """Run a system command checking only the status."""
    process = await asyncio.create_subprocess_exec(
        *command,
        stdin=asyncio.subprocess.DEVNULL,
        stdout=asyncio.subprocess.DEVNULL,
        stderr=asyncio.subprocess.DEVNULL,
        close_fds=False,
    )
    await process.wait()
    return process.returncode == 0


async def async_run_system_command(command: Iterable[str]) -> tuple[bool, bytes, bytes]:
    """Run a system command and return a tuple of returncode, stdout, stderr."""
    process = await asyncio.create_subprocess_exec(
        *command,
        stdin=asyncio.subprocess.DEVNULL,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        close_fds=False,
    )
    stdout, stderr = await process.communicate()
    await process.wait()
    return process.returncode, stdout, stderr

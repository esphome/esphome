"""Rapid connect/disconnect stress test for ESPHome native API."""

import asyncio
import sys
import time

from aioesphomeapi import APIClient

HOST = "192.168.1.100"
PORT = 6053
PASSWORD = ""
NOISE_PSK = None
ITERATIONS = 500
CONCURRENCY = 4  # simultaneous connection attempts


async def connect_disconnect(client_id: int, iteration: int) -> tuple[int, bool, str]:
    """Connect and immediately disconnect."""
    cli = APIClient(HOST, PORT, PASSWORD, noise_psk=NOISE_PSK)
    try:
        await asyncio.wait_for(cli.connect(login=True), timeout=10)
        await cli.disconnect()
        return iteration, True, ""
    except Exception as e:
        return (
            iteration,
            False,
            f"client{client_id} iter{iteration}: {type(e).__name__}: {e}",
        )
    finally:
        await cli.disconnect(force=True)


async def main() -> None:
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else ITERATIONS
    concurrency = int(sys.argv[2]) if len(sys.argv) > 2 else CONCURRENCY

    print(f"Stress testing {HOST}:{PORT}")
    print(f"Iterations: {iterations}, Concurrency: {concurrency}")
    print()

    success = 0
    fail = 0
    errors: list[str] = []
    start = time.monotonic()

    sem = asyncio.Semaphore(concurrency)

    async def run(client_id: int, iteration: int) -> tuple[int, bool, str]:
        async with sem:
            return await connect_disconnect(client_id, iteration)

    tasks = [asyncio.create_task(run(i % concurrency, i)) for i in range(iterations)]

    for coro in asyncio.as_completed(tasks):
        iteration, ok, err = await coro
        if ok:
            success += 1
        else:
            fail += 1
            errors.append(err)
        total = success + fail
        if total % 10 == 0 or not ok:
            elapsed = time.monotonic() - start
            rate = total / elapsed if elapsed > 0 else 0
            print(f"[{total}/{iterations}] ok={success} fail={fail} ({rate:.1f}/s)")
            if err:
                print(f"  ERROR: {err}")

    elapsed = time.monotonic() - start
    print()
    print(f"Done in {elapsed:.1f}s")
    print(f"Success: {success}, Failed: {fail}, Rate: {iterations / elapsed:.1f}/s")

    if errors:
        print("\nLast 10 errors:")
        for e in errors[-10:]:
            print(f"  {e}")

    sys.exit(1 if fail > 0 else 0)


if __name__ == "__main__":
    asyncio.run(main())

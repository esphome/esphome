"""
Rapid native API connect/disconnect stress test (from esphome#14687 and esphome#14706).

usage: connstress.py HOST [ITERATIONS] [CONCURRENCY]   (default 500 iterations, 4 at a time)
env:   API_KEY (base64 noise key, omit for plaintext), API_PORT (default 6053), API_PASSWORD
Each attempt connects with login, disconnects and counts a failure on any error or a 10 s timeout.
Exits 1 when any attempt failed. Watch the serial log for crashes: the client only sees resets.
"""

import asyncio
import os
import sys
import time

from aioesphomeapi import APIClient

HOST = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
PORT = int(os.environ.get("API_PORT", "6053"))
PASSWORD = os.environ.get("API_PASSWORD", "")
NOISE_PSK = os.environ.get("API_KEY") or None


async def connect_disconnect(client_id: int, iteration: int) -> tuple[int, bool, str]:
    cli = APIClient(HOST, PORT, PASSWORD, noise_psk=NOISE_PSK)
    try:
        await asyncio.wait_for(cli.connect(login=True), timeout=10)
        await cli.disconnect()
        return iteration, True, ""
    except Exception as e:  # noqa: BLE001  every failure is a counted result
        return (
            iteration,
            False,
            f"client{client_id} iter{iteration}: {type(e).__name__}: {e}",
        )
    finally:
        await cli.disconnect(force=True)


async def main() -> None:
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 500
    concurrency = int(sys.argv[3]) if len(sys.argv) > 3 else 4
    print(f"Iterations: {iterations}, Concurrency: {concurrency}", flush=True)

    success = fail = 0
    errors: list[str] = []
    start = time.monotonic()
    sem = asyncio.Semaphore(concurrency)

    async def run(client_id: int, iteration: int) -> tuple[int, bool, str]:
        async with sem:
            return await connect_disconnect(client_id, iteration)

    tasks = [asyncio.create_task(run(i % concurrency, i)) for i in range(iterations)]
    for coro in asyncio.as_completed(tasks):
        _, ok, err = await coro
        if ok:
            success += 1
        else:
            fail += 1
            errors.append(err)
        total = success + fail
        if total % 10 == 0 or not ok:
            rate = total / max(time.monotonic() - start, 1e-6)
            print(
                f"[{total}/{iterations}] ok={success} fail={fail} ({rate:.1f}/s)",
                flush=True,
            )
            if err:
                print(f"  ERROR: {err}", flush=True)

    elapsed = time.monotonic() - start
    print(f"\nDone in {elapsed:.1f}s")
    print(f"Success: {success}, Failed: {fail}, Rate: {iterations / elapsed:.1f}/s")
    if errors:
        print("\nLast 10 errors:")
        for e in errors[-10:]:
            print(f"  {e}")
    print("RESULT", "PASS" if fail == 0 else "FAIL")
    sys.exit(1 if fail else 0)


if __name__ == "__main__":
    asyncio.run(main())

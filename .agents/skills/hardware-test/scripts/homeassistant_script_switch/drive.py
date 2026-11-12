"""Press the device's buttons through Home Assistant and print the script and flag states after each step.

usage: drive.py TOKENFILE NAME   env: HA_URL (default http://127.0.0.1:8129)
NAME is the device name from hsw.yaml (button entity ids are button.<NAME>_long_on and so on).
"""

import pathlib
import sys
import time

sys.path.insert(
    0, str(pathlib.Path(__file__).resolve().parent.parent / "mqtt" / "ha_discovery")
)
from ha_rest import req_or_exit  # noqa: E402

tok = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip()
name = sys.argv[2].replace("-", "_")  # entity ids use underscores
t0 = time.monotonic()


def state(entity: str) -> str:
    return req_or_exit(f"/api/states/{entity}", token=tok)["state"]


def show(tag: str) -> None:
    print(
        f"{time.monotonic() - t0:6.1f}s {tag:30s} long_job={state('script.long_job'):4s}"
        f" quick_job={state('script.quick_job'):4s} flag={state('input_boolean.script_ran')}",
        flush=True,
    )


def call(domain: str, action: str, entity: str, wait: float = 3) -> None:
    req_or_exit(f"/api/services/{domain}/{action}", {"entity_id": entity}, token=tok)
    time.sleep(wait)


show("start")
call("button", "press", f"button.{name}_long_on")
show("device Long on")
call("button", "press", f"button.{name}_long_off")
show("device Long off")
call("input_boolean", "turn_off", "input_boolean.script_ran", 1)
call("button", "press", f"button.{name}_long_on")
show("device Long on again")
time.sleep(20)
show("script finished on its own")
call("button", "press", f"button.{name}_quick_on")
show("device Quick on")
call("script", "turn_on", "script.long_job")
show("HA starts the script")

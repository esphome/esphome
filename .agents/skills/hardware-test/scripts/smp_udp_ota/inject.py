"""Run esphome with the SMP reset request faulted on the host, to drive the SMP over UDP reset recovery.

usage: inject.py MODE ESPHOME_ARGS...   (for example: inject.py lostreply -s ver v2 upload dev.yaml --device ADDR)

MODE lostreply: the first reset is sent and answered, then the reply is dropped (TimeoutError).
MODE lostreq:   the first reset is never sent (TimeoutError at once).
MODE lostreq2:  no reset is ever sent.
Run it with the checkout's python and PYTHONPATH set to the tree under test; every INJECT line marks a fault.
"""

import runpy
import sys

from smp.os_management import ResetWriteRequest
from smpclient import SMPClient

MODES = ("lostreply", "lostreq", "lostreq2")
if len(sys.argv) < 2 or sys.argv[1] not in MODES:
    sys.exit(__doc__)
MODE = sys.argv.pop(1)
_original = SMPClient.request
_state = {"faulted": False}


async def request(self, req, *args, **kwargs):
    if isinstance(req, ResetWriteRequest) and (
        not _state["faulted"] or MODE == "lostreq2"
    ):
        _state["faulted"] = True
        if MODE == "lostreply":
            await _original(self, req, *args, **kwargs)
            print("INJECT reset sent and answered, reply dropped", flush=True)
        else:
            print("INJECT reset request dropped, not sent", flush=True)
        raise TimeoutError
    if isinstance(req, ResetWriteRequest):
        print("INJECT another reset sent", flush=True)
    return await _original(self, req, *args, **kwargs)


SMPClient.request = request
sys.argv = ["esphome", *sys.argv[1:]]
runpy.run_module("esphome", run_name="__main__")

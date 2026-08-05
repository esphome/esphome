"""Run the upload command dispatch path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
The bundle suffix check in run_esphome reads BUNDLE_EXTENSION from
esphome.const; if someone re-adds the esphome.bundle import for it,
this reports the leak.
"""

import sys
from unittest.mock import patch

from _leak_report import print_leaked_modules

from esphome import __main__ as main_mod

# The watch list rides on argv for _leak_report; strip it before
# argparse sees it.
watch_list = sys.argv[1:]
sys.argv = [sys.argv[0]]

dispatched = {}


def fake_upload(args, config):
    dispatched["config"] = config
    return 0


with (
    patch("esphome.compiled_config.load_compiled_config", return_value={"esphome": {}}),
    patch.dict(main_mod.POST_CONFIG_ACTIONS, {"upload": fake_upload}),
):
    exit_code = main_mod.run_esphome(
        ["esphome", "upload", "test.yaml", "--device", "192.0.2.1"]
    )

# Fail loudly if the dispatch never reached the command; otherwise an
# empty leak list could just mean nothing ran. Explicit exits rather than
# asserts so PYTHONOPTIMIZE in the ambient environment can't strip them.
if exit_code != 0:
    sys.exit(f"run_esphome exited {exit_code} before dispatching upload")
if dispatched.get("config") != {"esphome": {}}:
    sys.exit(f"upload command never received the cached config: {dispatched!r}")

sys.argv = [sys.argv[0], *watch_list]
print_leaked_modules()

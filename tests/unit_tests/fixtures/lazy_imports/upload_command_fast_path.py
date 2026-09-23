"""Run the upload command dispatch path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
Covers three fast-path claims: the bundle suffix check in run_esphome reads
BUNDLE_EXTENSION from esphome.const without importing esphome.bundle, the
validated-config cache parse stays voluptuous free, and the JSON cache
(lambda sentinel included) resolves without pyyaml or esphome.yaml_util.
"""

import json
import os
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

from _leak_report import print_leaked_modules
from _storage import make_storage

# Everything imported past this point is the code under test; the pop
# below must only drop what the setup itself preloaded, or it would
# hide modules the dispatch chain pulls in (tarfile has no other guard).
_FIXTURE_PRELOADED = frozenset(sys.modules)

from esphome import __main__ as main_mod  # noqa: E402
from esphome.const import __version__ as ESPHOME_VERSION  # noqa: E402

CONFIG_TEXT = "esphome:\n  name: t\n"
LAMBDA_BODY = 'ESP_LOGD("t", "x");'

# An ambient data-dir override would relocate the storage tree away
# from the tmp config dir this fixture builds.
os.environ.pop("ESPHOME_DATA_DIR", None)
os.environ.pop("ESPHOME_IS_HA_ADDON", None)

with tempfile.TemporaryDirectory() as _td:
    tmp = Path(_td)
    conf_path = tmp / "test.yaml"
    conf_path.write_text(CONFIG_TEXT)

    storage_dir = tmp / ".esphome" / "storage"
    storage_dir.mkdir(parents=True)
    # The cache carries a lambda sentinel so loading revives a real Lambda
    # on the fast path. The sidecar is written to the layout
    # ext_storage_path resolves once run_esphome sets CORE.config_path;
    # going through CORE here would be circular.
    cache_path = storage_dir / "test.yaml.validated.json"
    cache_path.write_text(
        json.dumps(
            {
                "v": 1,
                "esphome": ESPHOME_VERSION,
                "config": {
                    "esphome": {"name": "t"},
                    "script": [{"lambda": {"__esphome_lambda__": LAMBDA_BODY}}],
                },
            }
        )
    )
    os.utime(cache_path)  # keep the cache at least as fresh as the source
    make_storage().save(storage_dir / "test.yaml.json")

    dispatched = {}

    def fake_upload(args, config):
        dispatched["config"] = config
        return 0

    # This setup pre-imports some watched stdlib modules (tempfile above,
    # write_file inside make_storage().save(), unittest.mock -> asyncio ->
    # subprocess). Drop exactly those so only a genuine dispatch-time
    # re-import is reported; live objects keep their references, so
    # cleanup still works. Module-level re-imports are out of reach here
    # (esphome.__main__ is already loaded) — the bare-import check in
    # test_lazy_imports owns that contract.
    for module in sys.argv[1:]:
        if module in _FIXTURE_PRELOADED:
            sys.modules.pop(module, None)

    with patch.dict(main_mod.POST_CONFIG_ACTIONS, {"upload": fake_upload}):
        exit_code = main_mod.run_esphome(
            ["esphome", "upload", str(conf_path), "--device", "192.0.2.1"]
        )

    # Fail loudly if the fast path didn't do its work; otherwise an empty
    # leak list could just mean nothing ran. Explicit exits rather than
    # asserts so PYTHONOPTIMIZE in the ambient environment can't strip them.
    if exit_code != 0:
        sys.exit(f"run_esphome exited {exit_code} before dispatching upload")
    config = dispatched.get("config")
    if config is None or config.get("esphome") != {"name": "t"}:
        sys.exit(f"cache did not resolve through the fast path: {dispatched!r}")
    from esphome.core import Lambda

    revived = config["script"][0]["lambda"]
    if not isinstance(revived, Lambda) or revived.value != LAMBDA_BODY:
        sys.exit(f"lambda sentinel did not revive: {revived!r}")

    print_leaked_modules()

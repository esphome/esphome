"""Test-manifest overrides for the captive_portal C++ unit tests.

``json_escape`` lives in a standalone, dependency-free header
(``esphome/components/captive_portal/json_escape.h``). The rest of the
captive_portal component and its auto-loaded dependencies (``web_server_base``,
``ota.web_server``) do not build for the ``host`` platform that the C++ unit
test harness targets. Strip those away and replace the real schema -- which is
restricted to non-host platforms via ``cv.only_on`` and requires a
``web_server_base`` instance via ``use_id`` -- with an empty one so the host
test config validates. ``to_code`` stays suppressed (the default), so
``USE_CAPTIVE_PORTAL`` is never defined and ``captive_portal.cpp`` compiles to an
empty translation unit; only ``json_escape.h`` is exercised by the test.
"""

import esphome.config_validation as cv
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.auto_load = []
    manifest.dependencies = []
    manifest.config_schema = cv.Schema({})
    manifest.final_validate_schema = None

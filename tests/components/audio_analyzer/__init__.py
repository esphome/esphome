"""Build the microphone analyzer and a log listener into the unit tests."""

import esphome.codegen as cg
from esphome.components.logger import request_log_listener
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config: ConfigType) -> None:
        # test_microphone.cpp runs the microphone analyzer on a fake microphone
        cg.add_define("USE_MICROPHONE")
        # The tests check the warnings through a log listener
        request_log_listener()

    manifest.multi_conf = False
    manifest.to_code = to_code
    manifest.dependencies = manifest.dependencies + ["microphone"]

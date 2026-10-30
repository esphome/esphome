import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real to_code requests these from ota_to_code when on_* automations are configured
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_OTA_STATE_LISTENER")
        cg.add_define("OTA_STATE_CALLBACK_COUNT", 4)

    manifest.to_code = to_code_testing

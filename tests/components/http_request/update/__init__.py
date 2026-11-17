import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # setup() registers the entity as an OTA state listener, which the real to_code
    # requests through ota.request_ota_state_listeners().
    async def to_code_testing(config):
        cg.add_define("USE_OTA_STATE_LISTENER")

    manifest.to_code = to_code_testing

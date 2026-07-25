import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # resolve_irk() is compiled only when a sensor configures irk:
    # (request_irk_support() emits USE_BLE_DEVICE_IRK). The unit-test build has
    # no sensors, so emit the define here to put the real IRK path under test.
    async def to_code_testing(config):
        cg.add_define("USE_BLE_DEVICE_IRK")

    manifest.to_code = to_code_testing

import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # resolve_irk() is compiled only when a sensor configures irk:
    # (request_irk_support() emits USE_BLE_DEVICE_IRK). The unit-test build has
    # no sensors, so emit the define here to put the real IRK path under test.
    # Likewise the scan-response merger (emitted by the split-report trackers)
    # and the listener vector it dispatches into (codegen-sized by consumers).
    async def to_code_testing(config):
        cg.add_define("USE_BLE_DEVICE_IRK")
        cg.add_define("USE_BLE_SCAN_RESPONSE_MERGER")
        cg.add_define("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT", 4)

    manifest.to_code = to_code_testing

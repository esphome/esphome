import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # close_service_batch compiles only under BLUETOOTH_CONNECTION_HAS_GATT;
    # emit the backend define so the host build exercises it.
    async def to_code_testing(config):
        cg.add_define("USE_BLE_GATT_CLIENT")
        # The api protobuf bluetooth messages are behind the proxy define.
        cg.add_define("USE_BLUETOOTH_PROXY")
        cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)
        cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 1)

    manifest.to_code = to_code_testing
    # The batcher sizes api protobuf messages.
    manifest.dependencies = manifest.dependencies + ["api"]

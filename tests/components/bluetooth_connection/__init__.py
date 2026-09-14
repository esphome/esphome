import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # close_service_batch compiles only under USE_BLUETOOTH_PROXY_CONNECTIONS;
    # emit the backend define so the host build exercises it.
    async def to_code_testing(config):
        # These defines are global to the merged host test binary. The api sources are
        # compiled in it too (the api tests define USE_API), and USE_BLUETOOTH_PROXY would make
        # them include and call bluetooth_proxy, which has no host build without a BLE hub.
        cg.add_define("USE_BLE_GATT_CLIENT")
        cg.add_define("USE_BLE_GATT_CLIENT_STUB_BACKEND")
        # Gates the connection half of the API surface, which is what
        # close_service_batch and the GATT response types live behind.
        cg.add_define("USE_BLUETOOTH_PROXY_CONNECTIONS")
        cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)
        cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 1)

    manifest.to_code = to_code_testing
    # The batcher sizes api protobuf messages.
    manifest.dependencies = manifest.dependencies + ["api"]

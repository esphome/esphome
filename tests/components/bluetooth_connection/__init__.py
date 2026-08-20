import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # close_service_batch compiles only under USE_BLUETOOTH_PROXY_CONNECTIONS;
    # emit the backend define so the host build exercises it.
    async def to_code_testing(config):
        # These defines are global to the merged host test binary; safe
        # because no co-compiled test observes them.
        cg.add_define("USE_BLE_GATT_CLIENT")
        cg.add_define("USE_BLE_GATT_CLIENT_STUB_BACKEND")
        cg.add_define("USE_BLUETOOTH_PROXY")
        # Gates the connection half of the API surface, which is what
        # close_service_batch and the GATT response types live behind.
        cg.add_define("USE_BLUETOOTH_PROXY_CONNECTIONS")
        cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)
        cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 1)

    manifest.to_code = to_code_testing
    # The batcher sizes api protobuf messages.
    manifest.dependencies = manifest.dependencies + ["api"]

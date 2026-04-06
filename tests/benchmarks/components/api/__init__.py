import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # api must run its to_code to define USE_API, USE_API_PLAINTEXT,
    # and add the noise-c library dependency.
    manifest.enable_codegen()

    original_to_code = manifest.to_code

    async def to_code(config):
        await original_to_code(config)
        # Enable BLE proto message types for benchmarks.  The real
        # bluetooth_proxy component is ESP32-only; a lightweight stub
        # header in tests/benchmarks/stubs/ satisfies the include.
        cg.add_define("USE_BLUETOOTH_PROXY")
        cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 3)
        cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)

    manifest.to_code = to_code

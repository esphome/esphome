import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # api must run its to_code to define USE_API, USE_API_PLAINTEXT,
    # and add the noise-c library dependency.
    manifest.enable_codegen()

    original_to_code = manifest.to_code

    async def to_code(config):
        await original_to_code(config)
        # Enable proxy proto message types for benchmarks.  The real
        # components have hardware dependencies (BLE/UART/RMT); lightweight
        # stub headers in tests/benchmarks/stubs/ satisfy the includes.
        cg.add_define("USE_BLUETOOTH_PROXY")
        cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 3)
        cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)
        cg.add_define("USE_ZWAVE_PROXY")
        cg.add_define("USE_INFRARED")
        cg.add_define("USE_IR_RF")
        cg.add_define("USE_RADIO_FREQUENCY")
        cg.add_define("USE_SERIAL_PROXY")
        cg.add_define("SERIAL_PROXY_COUNT", 0)
        cg.add_define("ESPHOME_ENTITY_INFRARED_COUNT", 0)
        cg.add_define("ESPHOME_ENTITY_RADIO_FREQUENCY_COUNT", 0)

    manifest.to_code = to_code

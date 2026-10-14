import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness gives a multi-instance component an empty list and never calls its to_code,
    # so take a single empty config instead and emit what the proxy messages and handlers
    # need. The USB identity stays out, since the USB channel exists on the ESP32 only.
    manifest.multi_conf = False

    async def to_code_testing(config):
        cg.add_define("USE_SERIAL_PROXY")
        cg.add_define("SERIAL_PROXY_COUNT", 1)

    manifest.to_code = to_code_testing

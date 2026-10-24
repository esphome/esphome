import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The raw callback is only compiled in when a config uses on_raw_value.
    async def to_code_testing(config):
        cg.add_define("USE_BINARY_SENSOR_RAW_CALLBACK")

    manifest.to_code = to_code_testing

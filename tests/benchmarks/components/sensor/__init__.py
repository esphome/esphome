import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Enable real codegen for sensor publish_state benchmarks,
    # and also define USE_SENSOR_FILTER for filter benchmarks
    # (the real to_code only adds it when filters are configured).
    manifest.enable_codegen()

    real_to_code = manifest.to_code

    async def to_code(config):
        await real_to_code(config)
        cg.add_define("USE_SENSOR_FILTER")

    manifest.to_code = to_code

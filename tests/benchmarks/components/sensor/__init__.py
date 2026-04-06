import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Sensor filter benchmarks need USE_SENSOR_FILTER defined.
    # We use a custom to_code instead of enable_codegen() to avoid
    # pulling in the full sensor component setup.
    async def to_code(config):
        cg.add_define("USE_SENSOR_FILTER")

    manifest.to_code = to_code

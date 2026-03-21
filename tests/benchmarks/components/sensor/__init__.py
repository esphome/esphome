import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Get the real to_code from the wrapped manifest
    wrapped = object.__getattribute__(manifest, "_wrapped")
    original_to_code = wrapped.to_code

    async def to_code_with_filter(config):
        await original_to_code(config)
        # Enable sensor filter support so filter benchmarks can compile
        cg.add_define("USE_SENSOR_FILTER")

    manifest.to_code = to_code_with_filter

import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.enable_codegen()

    original_to_code = manifest.to_code

    async def to_code(config: ConfigType) -> None:
        await original_to_code(config)
        # The filter benchmarks need filter.cpp, which is only built with this define
        cg.add_define("USE_BINARY_SENSOR_FILTER")

    manifest.to_code = to_code

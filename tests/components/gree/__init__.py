import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD sits on the climate platform, which the unit test build does not load.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate", "switch"]

    async def to_code_testing(config: ConfigType) -> None:
        # YAW1F follow-up framing must be present in host C++ tests.
        cg.add_define("USE_GREE_YAW1F")

    manifest.to_code = to_code_testing

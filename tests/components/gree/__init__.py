import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD sits on the climate platform, which the unit test build does not load.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate", "switch"]

    async def to_code_testing(config: ConfigType) -> None:
        # These paths are define-gated; emit the defines so the tests cover them.
        cg.add_define("USE_GREE_FAHRENHEIT")
        cg.add_define("USE_GREE_FEATURE_SWITCH")
        cg.add_define("USE_GREE_RECEIVER")
        cg.add_define("USE_GREE_YAW1F")

    manifest.to_code = to_code_testing

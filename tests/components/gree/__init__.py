import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD sits on the climate platform, which the unit test build does not load.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]

    async def to_code_testing(config: ConfigType) -> None:
        # Fahrenheit support is define-gated; emit it so the tests cover that path.
        cg.add_define("USE_GREE_FAHRENHEIT")
        # The YAW1F follow-up message is compiled in only for configs that use that model
        cg.add_define("USE_GREE_YAW1F")

    manifest.to_code = to_code_testing

import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD sits on the climate platform, which the unit test build does not load.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]

    # Fahrenheit support is define-gated; emit it so the tests cover that path.
    async def to_code_testing(config):
        cg.add_define("USE_GREE_FAHRENHEIT")

    manifest.to_code = to_code_testing

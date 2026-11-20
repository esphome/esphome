import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        # The accessor hooks that partitions use, and the gamma table of the light under test
        cg.add_define("USE_LIGHT_PARTITION")
        cg.add_define("USE_LIGHT_GAMMA_LUT")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["light"]

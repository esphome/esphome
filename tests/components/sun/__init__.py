import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        cg.add_define("SUN_ELEVATION_CALLBACK_COUNT", 4)

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["time"]

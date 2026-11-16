import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        cg.add_define("USE_SUN_ELEVATION_POLL")
        cg.add_define("SUN_SUNRISE_AUTOMATION_COUNT", 4)
        cg.add_define("SUN_SUNSET_AUTOMATION_COUNT", 4)

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["time"]

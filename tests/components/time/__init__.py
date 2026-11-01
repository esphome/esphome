import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config):
        cg.add_define("USE_TIME_TIMEZONE")
        cg.add_define("USE_TIME_TRIGGERS")
        cg.add_define("TIME_ON_TIME_COUNT", 1)

    manifest.to_code = to_code

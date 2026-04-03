import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        cg.add_define("MITSUBISHI_CN105_UNIT_TEST")

    manifest.to_code = to_code_testing

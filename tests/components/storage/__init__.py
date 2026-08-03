import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config):
        cg.add_build_flag("-DUSE_STORAGE_REGEX_EXTRACT")

    manifest.to_code = to_code

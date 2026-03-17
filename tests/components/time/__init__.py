import esphome.codegen as cg
from esphome.testing import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config):
        cg.add_build_flag("-DUSE_TIME_TIMEZONE")

    manifest.to_code = to_code

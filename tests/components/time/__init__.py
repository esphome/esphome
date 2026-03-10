import esphome.codegen as cg
from esphome.loader import TestingComponentManifest


def override_manifest(manifest: TestingComponentManifest) -> None:
    async def to_code(config):
        cg.add_build_flag("-DUSE_TIME_TIMEZONE")

    manifest.to_code = to_code

"""Run the production player on the host against a FreeRTOS queue double."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["media_player", "media_source", "speaker"]
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_build_flag(f"-I{CORE.relative_src_path('speaker_source', '__sdk')}")

    manifest.to_code = to_code_testing

"""Compile the complete P4 driver on host with public SDK boundary doubles."""

import os

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["display"]
    manifest.auto_load = []
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_ESP32_VARIANT_ESP32P4")
        cg.add_define("USE_MIPI_DSI_RGB565_OWNED_TRANSFORM")
        cg.add_build_flag("-DCONFIG_CACHE_L2_CACHE_LINE_SIZE=64")
        major = os.environ.get("MIPI_TEST_IDF_MAJOR", "5")
        patch = os.environ.get("MIPI_TEST_IDF_PATCH", "3" if major == "6" else "6")
        cg.add_build_flag(f"-DMIPI_TEST_IDF_MAJOR={major}")
        cg.add_build_flag(f"-DMIPI_TEST_IDF_PATCH={patch}")
        cg.add_build_flag(f"-I{CORE.relative_src_path('mipi_dsi', '__sdk')}")

    manifest.to_code = to_code_testing

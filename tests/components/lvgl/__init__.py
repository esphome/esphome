import esphome.codegen as cg
from esphome.components.lvgl import LVGL_VERSION
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real to_code needs a display and widgets. The unit tests only need the LVGL
    # headers and the animation code.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_library("lvgl/lvgl", LVGL_VERSION)
        cg.add_build_flag("-DLV_CONF_SKIP=1")
        cg.add_define("USE_LVGL_ANIMATION")

    manifest.to_code = to_code_testing

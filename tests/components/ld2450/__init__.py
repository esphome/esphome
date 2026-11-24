from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # ld2450 is MULTI_CONF, so no codegen runs here to keep polygon.cpp; the tests need it
    manifest.resources = manifest.resources + [
        FileResource("esphome.components.ld2450", "polygon.cpp")
    ]

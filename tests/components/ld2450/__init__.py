from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # polygon.cpp only builds when zones are configured. ld2450 is MULTI_CONF, so the harness gives it no
    # config and no define; the host tests use Polygon directly, so add the source back for them.
    manifest.resources = manifest.resources + [
        FileResource("esphome.components.ld2450", "polygon.cpp")
    ]

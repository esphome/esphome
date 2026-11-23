# This file's presence makes pytest treat this directory as a package named
# "virtual_uart"; required for cpp unit testing.
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The gtests build their own instances.
    manifest.multi_conf = False

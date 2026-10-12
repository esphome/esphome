# This file's presence makes pytest treat this directory as a package named
# "uart_tcp"; required for cpp unit testing.
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.multi_conf = False

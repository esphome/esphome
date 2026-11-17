from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The bus class lives in the hub's directory and builds against the one_wire base class.
    manifest.dependencies = manifest.dependencies + ["one_wire"]

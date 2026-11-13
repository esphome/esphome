from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # test_can_send_action.cpp builds against the API server
    manifest.dependencies = manifest.dependencies + ["api"]

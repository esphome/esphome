from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    # USE_UART_DEBUGGER and USE_UART_RX_SINK compile the virtual UART's debugger and sink paths for the tests.
    add_defines_to_code(manifest, "USE_UART_DEBUGGER", "USE_UART_RX_SINK")

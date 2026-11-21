from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    # USE_UART_TX_BREAK: the host tests cover the default of set_tx_break().
    add_defines_to_code(manifest, "USE_UART_TX_BREAK")

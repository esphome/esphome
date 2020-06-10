from typing import Text

import hypothesis.strategies._internal.core as st
from hypothesis.strategies._internal.strategies import SearchStrategy


@st.defines_strategy_with_reusable_values
def mac_addr_strings():
    # type: () -> SearchStrategy[Text]
    """A strategy for MAC address strings.

    This consists of six strings representing integers [0..255],
    without zero-padding, joined by dots.
    """
    return st.builds("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}".format, *(6 * [st.integers(0, 255)]))

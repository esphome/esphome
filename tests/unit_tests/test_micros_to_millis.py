"""Tests for micros_to_millis Euclidean decomposition.

Verifies that the Python equivalent of the C++ micros_to_millis() helper
in esphome/core/helpers.h matches the reference (us // 1000) across
edge cases and overflow boundaries. Tests both 32-bit and 64-bit variants.
"""

import pytest

# Constants matching the C++ implementation (shift3+div125 variant)
D = 125
Q = (1 << 32) // D  # 34359738
R = (1 << 32) % D  # 46
UINT32_MAX = 0xFFFFFFFF
UINT64_MAX = 0xFFFFFFFFFFFFFFFF


def micros_to_millis(us: int) -> int:
    """Convert microseconds to 32-bit milliseconds using Euclidean decomposition."""
    x = us >> 3
    lo = x & UINT32_MAX
    hi = (x >> 32) & UINT32_MAX
    adj = (hi * R + lo) & UINT32_MAX
    if adj < lo:
        return (hi * Q + (adj + R) // D + Q) & UINT32_MAX
    return (hi * Q + adj // D) & UINT32_MAX


def micros_to_millis_64(us: int) -> int:
    """Convert microseconds to 64-bit milliseconds using Euclidean decomposition."""
    x = us >> 3
    lo = x & UINT32_MAX
    hi = (x >> 32) & UINT32_MAX
    adj = (hi * R + lo) & UINT32_MAX
    if adj < lo:
        return (hi * Q + (adj + R) // D + Q) & UINT64_MAX
    return (hi * Q + adj // D) & UINT64_MAX


def reference_32(us: int) -> int:
    """Reference: truncated 32-bit result of us / 1000."""
    return (us // 1000) & UINT32_MAX


def reference_64(us: int) -> int:
    """Reference: 64-bit result of us / 1000."""
    return us // 1000


BOUNDARY_VALUES = [
    0,
    1,
    999,
    1000,
    1001,
    7999,
    8000,
    8001,
    999_999,
    1_000_000,
    UINT32_MAX - 1,
    UINT32_MAX,
    UINT32_MAX + 1,
]

HI_VALUES = [1, 2, 100, 603, 1000, 5000, 10000, 14685, 0xFFFF]
LO_VALUES = [0, 1, 999, UINT32_MAX - 999, UINT32_MAX]
UPTIME_VALUES = [
    2_592_000_000_000,  # 30-day
    31_536_000_000_000,  # 1-year
    3_200_000_000_000_000_000,  # ~101,700 years (near safe limit)
]


# --- 32-bit tests ---


@pytest.mark.parametrize("us", BOUNDARY_VALUES, ids=lambda v: f"us={v}")
def test_32bit_boundary_values(us: int) -> None:
    assert micros_to_millis(us) == reference_32(us)


@pytest.mark.parametrize("hi", HI_VALUES, ids=lambda v: f"hi={v}")
@pytest.mark.parametrize("lo_offset", LO_VALUES, ids=lambda v: f"lo={v}")
def test_32bit_hi_lo_combinations(hi: int, lo_offset: int) -> None:
    us = (hi << 32) | lo_offset
    assert micros_to_millis(us) == reference_32(us)


@pytest.mark.parametrize("hi", [1, 50, 100, 500, 1000, 5000], ids=lambda v: f"hi={v}")
def test_32bit_carry_boundary(hi: int) -> None:
    """Test around the adj overflow boundary (hi * R + lo > UINT32_MAX)."""
    base = hi << 35
    hi_r = hi * R
    if hi_r < UINT32_MAX:
        threshold_lo = UINT32_MAX - hi_r
        for lo in [threshold_lo - 1, threshold_lo, threshold_lo + 1]:
            us = base | (lo << 3)
            assert micros_to_millis(us) == reference_32(us)


@pytest.mark.parametrize(
    "us", UPTIME_VALUES, ids=["30_days", "1_year", "near_safe_limit"]
)
def test_32bit_realistic_uptimes(us: int) -> None:
    assert micros_to_millis(us) == reference_32(us)


def test_32bit_shift_boundary_mod8() -> None:
    """Values where us % 8 varies — exercises the >>3 shift edge."""
    for base in [0, 1000, 8000, UINT32_MAX, 603 << 32]:
        for offset in range(8):
            us = base + offset
            assert micros_to_millis(us) == reference_32(us)


# --- 64-bit tests ---


@pytest.mark.parametrize("us", BOUNDARY_VALUES, ids=lambda v: f"us={v}")
def test_64bit_boundary_values(us: int) -> None:
    assert micros_to_millis_64(us) == reference_64(us)


@pytest.mark.parametrize("hi", HI_VALUES, ids=lambda v: f"hi={v}")
@pytest.mark.parametrize("lo_offset", LO_VALUES, ids=lambda v: f"lo={v}")
def test_64bit_hi_lo_combinations(hi: int, lo_offset: int) -> None:
    us = (hi << 32) | lo_offset
    assert micros_to_millis_64(us) == reference_64(us)


@pytest.mark.parametrize("hi", [1, 50, 100, 500, 1000, 5000], ids=lambda v: f"hi={v}")
def test_64bit_carry_boundary(hi: int) -> None:
    """Test around the adj overflow boundary for 64-bit result."""
    base = hi << 35
    hi_r = hi * R
    if hi_r < UINT32_MAX:
        threshold_lo = UINT32_MAX - hi_r
        for lo in [threshold_lo - 1, threshold_lo, threshold_lo + 1]:
            us = base | (lo << 3)
            assert micros_to_millis_64(us) == reference_64(us)


@pytest.mark.parametrize(
    "us", UPTIME_VALUES, ids=["30_days", "1_year", "near_safe_limit"]
)
def test_64bit_realistic_uptimes(us: int) -> None:
    assert micros_to_millis_64(us) == reference_64(us)


def test_64bit_shift_boundary_mod8() -> None:
    """Values where us % 8 varies — exercises the >>3 shift edge."""
    for base in [0, 1000, 8000, UINT32_MAX, 603 << 32]:
        for offset in range(8):
            us = base + offset
            assert micros_to_millis_64(us) == reference_64(us)


def test_64bit_preserves_upper_bits() -> None:
    """Verify 64-bit variant does not truncate large results."""
    # 30-day uptime: result > UINT32_MAX
    us = 2_592_000_000_000
    result = micros_to_millis_64(us)
    assert result == 2_592_000_000
    # 1-year uptime
    us = 31_536_000_000_000
    result = micros_to_millis_64(us)
    assert result == 31_536_000_000
    assert result > UINT32_MAX


# --- Shared tests ---


def test_constants_match() -> None:
    """Verify the Euclidean decomposition constants are correct."""
    assert Q == 34359738
    assert R == 46
    assert Q * D + R == (1 << 32)


def test_constexpr_values() -> None:
    """Values suitable for static_assert in C++ if made constexpr."""
    assert micros_to_millis(0) == 0
    assert micros_to_millis(999) == 0
    assert micros_to_millis(1000) == 1
    assert micros_to_millis_64(0) == 0
    assert micros_to_millis_64(999) == 0
    assert micros_to_millis_64(1000) == 1
    assert micros_to_millis_64(2_592_000_000_000) == 2_592_000_000


def test_32bit_and_64bit_agree_when_result_fits() -> None:
    """Both variants agree when result fits in 32 bits."""
    for us in [0, 1000, 999_999, UINT32_MAX]:
        assert micros_to_millis(us) == micros_to_millis_64(us)

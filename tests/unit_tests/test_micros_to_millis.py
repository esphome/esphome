"""Tests for micros_to_millis Euclidean decomposition.

Verifies that the Python equivalent of the C++ micros_to_millis() helper
in esphome/core/helpers.h matches the reference (us // 1000) across
edge cases and overflow boundaries.
"""

import pytest

# Constants matching the C++ implementation (shift3+div125 variant)
D = 125
Q = (1 << 32) // D  # 34359738
R = (1 << 32) % D  # 46
UINT32_MAX = 0xFFFFFFFF


def micros_to_millis(us: int) -> int:
    """Convert microseconds to milliseconds using Euclidean decomposition."""
    x = us >> 3
    lo = x & UINT32_MAX
    hi = (x >> 32) & UINT32_MAX
    adj = (hi * R + lo) & UINT32_MAX
    if adj < lo:
        return (hi * Q + (adj + R) // D + Q) & UINT32_MAX
    return (hi * Q + adj // D) & UINT32_MAX


def reference(us: int) -> int:
    """Reference implementation: truncated 32-bit result of us / 1000."""
    return (us // 1000) & UINT32_MAX


@pytest.mark.parametrize(
    "us",
    [
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
    ],
    ids=lambda v: f"us={v}",
)
def test_small_and_boundary_values(us):
    assert micros_to_millis(us) == reference(us)


@pytest.mark.parametrize(
    "hi",
    [1, 2, 100, 603, 1000, 5000, 10000, 14685, 0xFFFF],
    ids=lambda v: f"hi={v}",
)
@pytest.mark.parametrize(
    "lo_offset",
    [0, 1, 999, UINT32_MAX - 999, UINT32_MAX],
    ids=lambda v: f"lo={v}",
)
def test_hi_lo_combinations(hi, lo_offset):
    us = (hi << 32) | lo_offset
    assert micros_to_millis(us) == reference(us)


@pytest.mark.parametrize(
    "hi",
    [1, 50, 100, 500, 1000, 5000],
    ids=lambda v: f"hi={v}",
)
def test_carry_boundary(hi):
    """Test around the adj overflow boundary (hi * R + lo > UINT32_MAX)."""
    # After >>3, the decomposition uses R=46
    # Carry boundary for original us: compute where adj wraps
    # hi_shifted = (us >> 3) >> 32 = us >> 35
    # We construct us such that hi_shifted = hi
    base = hi << 35
    hi_r = hi * R
    if hi_r < UINT32_MAX:
        threshold_lo = UINT32_MAX - hi_r
        # Test around the boundary in the shifted domain
        for lo in [threshold_lo - 1, threshold_lo, threshold_lo + 1]:
            us = base | (lo << 3)  # Scale lo back to us domain
            assert micros_to_millis(us) == reference(us)


@pytest.mark.parametrize(
    "us",
    [
        # 30-day uptime in microseconds
        2_592_000_000_000,
        # 1-year uptime
        31_536_000_000_000,
        # Near safe limit (~101,700 years)
        3_200_000_000_000_000_000,
    ],
    ids=["30_days", "1_year", "near_safe_limit"],
)
def test_realistic_uptimes(us):
    assert micros_to_millis(us) == reference(us)


def test_shift_boundary_mod8():
    """Values where us % 8 varies — exercises the >>3 shift edge."""
    for base in [0, 1000, 8000, UINT32_MAX, 603 << 32]:
        for offset in range(8):
            us = base + offset
            assert micros_to_millis(us) == reference(us)


def test_constants_match():
    """Verify the Euclidean decomposition constants are correct."""
    assert Q == 34359738
    assert R == 46
    assert Q * D + R == (1 << 32)


def test_constexpr_values():
    """Values suitable for static_assert in C++ if made constexpr."""
    assert micros_to_millis(0) == 0
    assert micros_to_millis(999) == 0
    assert micros_to_millis(1000) == 1
    assert micros_to_millis(2_592_000_000_000) == 2_592_000_000

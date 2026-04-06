"""Tests for the logger component."""

import re


def test_logger_pre_setup_before_other_components(generate_main):
    """Logger::pre_setup() must be called before any other component is created.

    Log functions call global_logger->log_vprintf_() without a null check,
    so global_logger must be set before anything can log.
    """
    main_cpp = generate_main("tests/component_tests/logger/test_logger.yaml")

    # Find the logger's pre_setup() call specifically
    logger_pre_setup = re.search(r"logger_logger->pre_setup\(\)", main_cpp)
    if logger_pre_setup is None:
        # Fall back to finding any logger-related pre_setup
        logger_pre_setup = re.search(r"logger\w*->pre_setup\(\)", main_cpp)
    assert logger_pre_setup is not None, (
        "Logger pre_setup() not found in generated code"
    )

    # Find all "new " allocations (component creation)
    new_allocations = list(re.finditer(r"\bnew [\w:]+", main_cpp))
    # Find all "new(" allocations (component creation) and combine them
    new_allocations.extend(re.finditer(r"\bnew\([^)]+\) [\w:]+", main_cpp))
    # Sort allocations by position in the file
    new_allocations.sort(key=lambda m: m.start())
    assert len(new_allocations) > 0, "No component allocations found"

    # Separate logger and non-logger allocations
    logger_allocs = [a for a in new_allocations if "logger" in a.group().lower()]
    non_logger_allocs = [
        a
        for a in new_allocations
        if "logger" not in a.group().lower()
        # Skip placement new for App
        and "(&App)" not in main_cpp[max(0, a.start() - 5) : a.start()]
    ]

    assert len(logger_allocs) > 0, (
        f"Logger allocation not found in: {[a.group() for a in new_allocations]}"
    )
    assert len(non_logger_allocs) > 0, (
        "No non-logger component allocations found — "
        "add a component to test_logger.yaml so the ordering check is meaningful"
    )

    # All non-logger allocations must appear after logger pre_setup()
    for alloc in non_logger_allocs:
        assert alloc.start() > logger_pre_setup.start(), (
            f"Component allocation '{alloc.group()}' at position {alloc.start()} "
            f"appears before logger pre_setup() at position {logger_pre_setup.start()}"
        )

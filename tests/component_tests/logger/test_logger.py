"""Tests for the logger component."""

import re


def test_logger_pre_setup_before_other_components(generate_main):
    """Logger::pre_setup() must be called before any other component is created.

    Log functions call global_logger->log_vprintf_() without a null check,
    so global_logger must be set before anything can log.
    """
    main_cpp = generate_main("tests/component_tests/logger/test_logger.yaml")

    # Find the position of logger pre_setup
    pre_setup_match = re.search(r"->pre_setup\(\)", main_cpp)
    assert pre_setup_match is not None, "Logger pre_setup() not found in generated code"

    # Find all "new " allocations (component creation)
    new_allocations = list(re.finditer(r"\bnew [\w:]+", main_cpp))
    assert len(new_allocations) > 0, "No component allocations found"

    # Find the logger allocation
    logger_new = None
    for alloc in new_allocations:
        if "logger" in alloc.group():
            logger_new = alloc
            break

    assert logger_new is not None, (
        f"Logger allocation not found in: {[a.group() for a in new_allocations]}"
    )

    # All non-logger allocations must appear after pre_setup()
    for alloc in new_allocations:
        if alloc == logger_new:
            continue
        # Skip "new (&App)" placement new which is before logger
        if "(&App)" in main_cpp[max(0, alloc.start() - 5) : alloc.start()]:
            continue
        assert alloc.start() > pre_setup_match.start(), (
            f"Component allocation '{alloc.group()}' at position {alloc.start()} "
            f"appears before logger pre_setup() at position {pre_setup_match.start()}"
        )

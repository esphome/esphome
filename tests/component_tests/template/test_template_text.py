"""Tests for the template text component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_template_text_saver_uses_placement_new_with_templated_subclass(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Regression test for template text restore saver using placement new.

    When ``restore_value: true``, the saver is its own Pvariable with
    placement new: storage is sized for ``TextSaver<MAX_LENGTH>``, the
    declared pointer stays at ``TemplateTextSaverBase *`` for polymorphism,
    and the templated subclass constructor runs. A regression would either
    reintroduce the heap ``new TextSaver<...>()`` expression or size the
    storage for the base class and silently skip the subclass ctor.
    """
    main_cpp = generate_main(component_config_path("template_text_restore.yaml"))

    # Storage is sized and aligned for the templated subclass.
    assert "sizeof(template_::TextSaver<10>)" in main_cpp
    assert "alignas(template_::TextSaver<10>)" in main_cpp
    # Pointer declared as base type for polymorphism.
    assert (
        "static template_::TemplateTextSaverBase *const test_text_restore_value_saver"
        in main_cpp
    )
    # Placement new runs the templated subclass constructor.
    assert "new(test_text_restore_value_saver) template_::TextSaver<10>()" in main_cpp
    # Base-class default ctor must NOT be used.
    assert (
        "new(test_text_restore_value_saver) template_::TemplateTextSaverBase()"
        not in main_cpp
    )
    # No heap `new TextSaver<...>()` left over — the pre-fix pattern.
    assert "new template_::TextSaver<" not in main_cpp
    # Saver is wired into the text component.
    assert (
        "test_text_restore->set_value_saver(test_text_restore_value_saver)" in main_cpp
    )

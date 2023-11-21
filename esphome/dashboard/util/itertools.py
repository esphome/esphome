from __future__ import annotations

from collections.abc import Iterable
from functools import partial
from itertools import islice
from typing import Any


def take(take_num: int, iterable: Iterable) -> list[Any]:
    """Return first n items of the iterable as a list.

    From itertools recipes
    """
    return list(islice(iterable, take_num))


def chunked(iterable: Iterable, chunked_num: int) -> Iterable[Any]:
    """Break *iterable* into lists of length *n*.

    From more-itertools
    """
    return iter(partial(take, chunked_num, iter(iterable)), [])

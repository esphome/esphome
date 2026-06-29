"""ESPHome's ``cv.Schema``, built on probatio.

probatio validates the mappings; this module only layers on the few behaviors
ESPHome needs on top:

- every schema key must be wrapped in ``cv.Required`` or ``cv.Optional``;
- an undeclared ``id`` key is dropped silently, so ``!extend``/``!remove`` work on
  any list-based config without each component declaring an id in its schema;
- ``extra_schemas``: post-validation transforms applied once the mapping validates.

probatio provides the rest natively: the "did you mean?" close matches on an unknown
key (``ExtraKeysInvalid.candidates``), ``dictionary value`` error tagging, and
required/default handling.
"""

import probatio
from probatio import error as probatio_error

from esphome.const import CONF_ID
from esphome.schema_extractors import schema_extractor_extended

# Re-exported so existing imports keep working. probatio's ExtraKeysInvalid already
# carries the ``candidates`` close-match list that ESPHome renders in its errors.
ExtraKeysInvalid = probatio_error.ExtraKeysInvalid

# A bare value of one of these types is not a valid schema key in ESPHome; keys must
# be wrapped in Required/Optional. Mirrors voluptuous' ``primitive_types``.
_PRIMITIVE_TYPES = (str, int, float, bool, bytes, complex)

# A single shared marker for the silent ``id`` drop, reused across every compiled
# mapping instead of constructing a new one each time.
_REMOVE_ID = probatio.Remove(CONF_ID)


def ensure_multiple_invalid(err):
    if isinstance(err, probatio.MultipleInvalid):
        return err
    if isinstance(err, list):
        return probatio.MultipleInvalid(err)
    return probatio.MultipleInvalid([err])


# pylint: disable=protected-access, unidiomatic-typecheck
class _Schema(probatio.Schema):
    """Custom cv.Schema: ESPHome key rules, id-dropping, and extra schemas."""

    def __init__(
        self, schema, required=False, extra=probatio.PREVENT_EXTRA, extra_schemas=None
    ):
        super().__init__(schema, required=required, extra=extra)
        # List of extra schemas to apply after validation. Should be used sparingly,
        # as it's not a very probatio-way/clean way of doing things.
        self._extra_schemas = extra_schemas or []

    def _compile_dict(self, schema):
        # Check some things that ESPHome's schemas do not allow, mostly to keep the
        # schemas sane (these may be re-added if ever needed).
        for key in schema:
            if key is probatio.Extra:
                raise ValueError("ESPHome does not allow vol.Extra")
            if isinstance(key, probatio.Remove):
                raise ValueError("ESPHome does not allow vol.Remove")
            if isinstance(key, _PRIMITIVE_TYPES):
                raise ValueError(
                    "All schema keys must be wrapped in cv.Required or cv.Optional"
                )

        # Silently drop an undeclared 'id' on any dict so that !extend / !remove work
        # on every list-based config without requiring each component to declare an id
        # in its schema. With ALLOW_EXTRA the key is kept like any other extra; with
        # REMOVE_EXTRA it is dropped anyway, so only inject the Remove when it matters.
        # The injected key lives in the compiled candidates only, never in .schema, so
        # schema introspection (docs, schema dumping) stays clean.
        if self.extra != probatio.ALLOW_EXTRA and CONF_ID not in schema:
            schema = {**schema, _REMOVE_ID: object}

        return super()._compile_dict(schema)

    def __call__(self, data):
        res = super().__call__(data)
        for extra in self._extra_schemas:
            try:
                res = extra(res)
            except probatio.Invalid as err:
                raise ensure_multiple_invalid(err) from err
        return res

    def add_extra(self, validator):
        validator = _Schema(validator)
        self._extra_schemas.append(validator)
        return self

    def prepend_extra(self, validator):
        validator = _Schema(validator)
        self._extra_schemas.insert(0, validator)
        return self

    @schema_extractor_extended
    def extend(self, *schemas, **kwargs):
        extra = kwargs.pop("extra", None)
        if kwargs:
            raise ValueError
        if not schemas:
            return self.extend({})
        if len(schemas) != 1:
            ret = self
            for schema in schemas:
                ret = ret.extend(schema)
            return ret

        schema = schemas[0]
        extra_schemas = self._extra_schemas.copy()
        if isinstance(schema, _Schema):
            extra_schemas.extend(schema._extra_schemas)
        if isinstance(schema, probatio.Schema):
            schema = schema.schema
        ret = super().extend(schema, extra=extra)
        # probatio's extend already returns a fully compiled _Schema (type(self)).
        # Only rewrap (recompile) when there are extra_schemas to carry over;
        # otherwise return it as-is to avoid compiling the merged mapping twice.
        if not extra_schemas:
            return ret
        return _Schema(ret.schema, extra=ret.extra, extra_schemas=extra_schemas)

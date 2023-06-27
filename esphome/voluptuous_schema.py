import difflib
import itertools

import voluptuous as vol
from esphome.schema_extractors import schema_extractor_extended


class ExtraKeysInvalid(vol.Invalid):
    def __init__(self, *arg, **kwargs):
        self.candidates = kwargs.pop("candidates")
        vol.Invalid.__init__(self, *arg, **kwargs)


def ensure_multiple_invalid(err):
    if isinstance(err, vol.MultipleInvalid):
        return err
    return vol.MultipleInvalid(err)


# pylint: disable=protected-access, unidiomatic-typecheck
class _Schema(vol.Schema):
    """Custom cv.Schema that prints similar keys on error."""

    def __init__(
        self, schema, required=False, extra=vol.PREVENT_EXTRA, extra_schemas=None
    ):
        super().__init__(schema, required=required, extra=extra)
        # List of extra schemas to apply after validation
        # Should be used sparingly, as it's not a very voluptuous-way/clean way of
        # doing things.
        self._extra_schemas = extra_schemas or []

    def __call__(self, data):
        res = super().__call__(data)
        for extra in self._extra_schemas:
            try:
                res = extra(res)
            except vol.Invalid as err:
                # pylint: disable=raise-missing-from
                raise ensure_multiple_invalid(err)
        return res

    def _compile_mapping(self, schema, invalid_msg=None):
        invalid_msg = invalid_msg or "mapping value"

        # Check some things that ESPHome's schemas do not allow
        # mostly to keep the logic in this method sane (so these may be re-added if needed).
        for key in schema:
            if key is vol.Extra:
                raise ValueError("ESPHome does not allow vol.Extra")
            if isinstance(key, vol.Remove):
                raise ValueError("ESPHome does not allow vol.Remove")
            if isinstance(key, vol.primitive_types):
                raise ValueError(
                    "All schema keys must be wrapped in cv.Required or cv.Optional"
                )

        # Keys that may be required
        all_required_keys = {key for key in schema if isinstance(key, vol.Required)}

        # Keys that may have defaults
        # This is a list because sets do not guarantee insertion order
        all_default_keys = [key for key in schema if isinstance(key, vol.Optional)]

        # Recursively compile schema
        _compiled_schema = {}
        for skey, svalue in vol.iteritems(schema):
            new_key = self._compile(skey)
            new_value = self._compile(svalue)
            _compiled_schema[skey] = (new_key, new_value)

        # Sort compiled schema (probably not necessary for esphome, but leave it here just in case)
        candidates = list(
            vol.schema_builder._iterate_mapping_candidates(_compiled_schema)
        )

        # After we have the list of candidates in the correct order, we want to apply some
        # optimization so that each
        # key in the data being validated will be matched against the relevant schema keys only.
        # No point in matching against different keys
        additional_candidates = []
        candidates_by_key = {}
        for skey, (ckey, cvalue) in candidates:
            if type(skey) in vol.primitive_types:
                candidates_by_key.setdefault(skey, []).append((skey, (ckey, cvalue)))
            elif (
                isinstance(skey, vol.Marker)
                and type(skey.schema) in vol.primitive_types
            ):
                candidates_by_key.setdefault(skey.schema, []).append(
                    (skey, (ckey, cvalue))
                )
            else:
                # These are wildcards such as 'int', 'str', 'Remove' and others which should be
                # applied to all keys
                additional_candidates.append((skey, (ckey, cvalue)))

        key_names = []
        for skey in schema:
            if isinstance(skey, str):
                key_names.append(skey)
            elif isinstance(skey, vol.Marker) and isinstance(skey.schema, str):
                key_names.append(skey.schema)

        def validate_mapping(path, iterable, out):
            required_keys = all_required_keys.copy()

            # Build a map of all provided key-value pairs.
            # The type(out) is used to retain ordering in case a ordered
            # map type is provided as input.
            key_value_map = type(out)()
            for key, value in iterable:
                key_value_map[key] = value

            # Insert default values for non-existing keys.
            for key in all_default_keys:
                if (
                    not isinstance(key.default, vol.Undefined)
                    and key.schema not in key_value_map
                ):
                    # A default value has been specified for this missing key, insert it.
                    key_value_map[key.schema] = key.default()

            error = None
            errors = []
            for key, value in key_value_map.items():
                key_path = path + [key]
                # Optimization. Validate against the matching key first, then fallback to the rest
                relevant_candidates = itertools.chain(
                    candidates_by_key.get(key, []), additional_candidates
                )

                # compare each given key/value against all compiled key/values
                # schema key, (compiled key, compiled value)
                for skey, (ckey, cvalue) in relevant_candidates:
                    try:
                        new_key = ckey(key_path, key)
                    except vol.Invalid as e:
                        if len(e.path) > len(key_path):
                            raise
                        if not error or len(e.path) > len(error.path):
                            error = e
                        continue
                    # Backtracking is not performed once a key is selected, so if
                    # the value is invalid we immediately throw an exception.
                    exception_errors = []
                    try:
                        cval = cvalue(key_path, value)
                        out[new_key] = cval
                    except vol.MultipleInvalid as e:
                        exception_errors.extend(e.errors)
                    except vol.Invalid as e:
                        exception_errors.append(e)

                    if exception_errors:
                        for err in exception_errors:
                            if len(err.path) <= len(key_path):
                                err.error_type = invalid_msg
                            errors.append(err)
                        # If there is a validation error for a required
                        # key, this means that the key was provided.
                        # Discard the required key so it does not
                        # create an additional, noisy exception.
                        required_keys.discard(skey)
                        break

                    # Key and value okay, mark as found in case it was
                    # a Required() field.
                    required_keys.discard(skey)

                    break
                else:
                    if self.extra == vol.ALLOW_EXTRA:
                        out[key] = value
                    elif self.extra != vol.REMOVE_EXTRA:
                        if isinstance(key, str) and key_names:
                            matches = difflib.get_close_matches(key, key_names)
                            errors.append(
                                ExtraKeysInvalid(
                                    "extra keys not allowed",
                                    key_path,
                                    candidates=matches,
                                )
                            )
                        else:
                            errors.append(
                                vol.Invalid("extra keys not allowed", key_path)
                            )

            # for any required keys left that weren't found and don't have defaults:
            for key in required_keys:
                msg = getattr(key, "msg", None) or "required key not provided"
                errors.append(vol.RequiredFieldInvalid(msg, path + [key]))
            if errors:
                raise vol.MultipleInvalid(errors)

            return out

        return validate_mapping

    def add_extra(self, validator):
        validator = _Schema(validator)
        self._extra_schemas.append(validator)
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
        if isinstance(schema, vol.Schema):
            schema = schema.schema
        ret = super().extend(schema, extra=extra)
        return _Schema(ret.schema, extra=ret.extra, extra_schemas=self._extra_schemas)

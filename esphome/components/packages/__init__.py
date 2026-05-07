from collections import UserDict
from collections.abc import Callable
from functools import reduce
import logging
from pathlib import Path
from typing import Any

from esphome import git, yaml_util
from esphome.components.substitutions import (
    ContextVars,
    ErrList,
    push_context,
    raise_first_undefined,
    resolve_include,
    resolve_substitutions_block,
    substitute,
)
from esphome.components.substitutions.jinja import has_jinja
from esphome.config_helpers import Remove, merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_FILE,
    CONF_FILES,
    CONF_MIN_VERSION,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_SUBSTITUTIONS,
    CONF_URL,
    CONF_USERNAME,
    CONF_VARS,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)

DOMAIN = CONF_PACKAGES
# Guard against infinite include chains (e.g. A includes B includes A).
MAX_INCLUDE_DEPTH = 20

PackageCallback = Callable[
    [dict | str | yaml_util.IncludeFile, ContextVars | None, yaml_util.DocumentPath],
    dict,
]


def is_remote_package(package_config: dict) -> bool:
    """Returns True if the package_config is a remote package definition."""
    return CONF_URL in package_config


def is_package_definition(value: object) -> bool:
    """Returns True if the value looks like a package definition rather than a config fragment.

    Package definitions are IncludeFile objects, git URL shorthand strings, or
    remote package dicts (containing a ``url:`` key).  Config fragments are
    plain dicts that represent component configuration.
    """
    return isinstance(value, (yaml_util.IncludeFile, str)) or (
        isinstance(value, dict) and is_remote_package(value)
    )


def valid_package_contents(package_config: dict) -> dict:
    """Validate that a package looks like a plausible ESPHome config fragment.

    Rejects non-dict values, remote package schemas (which should have been
    handled earlier), non-string keys, and scalar values that aren't Jinja
    expressions. This is a lightweight check to catch obvious mistakes before
    full component validation runs later.
    """

    if not isinstance(package_config, dict):
        raise cv.Invalid("Package contents must be a dict")

    if is_remote_package(package_config):
        # Package contents must not contain a root `url:` key
        raise cv.Invalid("Remote package schema not expected here")

    # Validate manually since Voluptuous would regenerate dicts and lose metadata
    # such as ESPHomeDataBase
    for k, v in package_config.items():
        if not isinstance(k, str):
            raise cv.Invalid("Package content keys must be strings")
        if isinstance(v, (dict, list, Remove, yaml_util.IncludeFile)):
            continue  # e.g. script: [], psram: !remove, logger: {level: debug}, switch: !include switches.yaml
        if v is None:
            continue  # e.g. web_server:
        if isinstance(v, str) and has_jinja(v):
            # e.g: remote package shorthand:
            # package_name: github://esphome/repo/file.yaml@${ branch }, or:
            # switch: ${ expression that evals to a switch }
            continue

        raise cv.Invalid("Invalid component content in package definition")
    return package_config


def expand_file_to_files(config: dict):
    if CONF_FILE in config:
        new_config = config
        new_config[CONF_FILES] = [config[CONF_FILE]]
        del new_config[CONF_FILE]
        return new_config
    return config


def validate_yaml_filename(value):
    value = cv.string(value)

    if not (value.endswith(".yaml") or value.endswith(".yml")):
        raise cv.Invalid("Only YAML (.yaml / .yml) files are supported.")

    return value


def validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Git URL shorthand only for strings")

    git_file = git.GitFile.from_shorthand(value)

    conf = {
        CONF_URL: git_file.git_url,
        CONF_FILE: git_file.filename,
    }
    if git_file.ref:
        conf[CONF_REF] = git_file.ref

    return REMOTE_PACKAGE_SCHEMA(conf)


def deprecate_single_package(config: dict) -> dict:
    _LOGGER.warning(
        """
        Including a single package under `packages:`, i.e., `packages: !include mypackage.yaml` is deprecated.
        This method for including packages will go away in 2026.7.0
        Please use a list instead:

        packages:
          - !include mypackage.yaml

        See https://github.com/esphome/esphome/pull/12116
        """
    )
    return config


REMOTE_PACKAGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_URL): cv.url,
            cv.Optional(CONF_PATH): cv.string,
            cv.Optional(CONF_USERNAME): cv.string,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Exclusive(CONF_FILE, CONF_FILES): validate_yaml_filename,
            cv.Exclusive(CONF_FILES, CONF_FILES): cv.All(
                cv.ensure_list(
                    cv.Any(
                        validate_yaml_filename,
                        cv.Schema(
                            {
                                cv.Required(CONF_PATH): validate_yaml_filename,
                                cv.Optional(CONF_VARS, default={}): cv.Schema(
                                    {cv.string: object}
                                ),
                            }
                        ),
                    )
                ),
                cv.Length(min=1),
            ),
            cv.Optional(CONF_REF): cv.git_ref,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_FILE, CONF_FILES),
    expand_file_to_files,
)

PACKAGE_SCHEMA = cv.Any(  # A package definition is either:
    validate_source_shorthand,  # A git URL shorthand string that expands to a remote package schema, or
    REMOTE_PACKAGE_SCHEMA,  # a valid remote package schema, or
    yaml_util.IncludeFile,  # isinstance check — passes IncludeFile objects through unchanged, or:
    valid_package_contents,  # Something that at least looks like an actual package, e.g. {wifi:{ssid: xxx}}
    # which will have to be fully validated later as per each component's schema.
)

CONFIG_SCHEMA = cv.Any(  # under `packages:` we can have either:
    cv.Schema(
        {
            str: PACKAGE_SCHEMA,  # a named dict of package definitions, or
        }
    ),
    [PACKAGE_SCHEMA],  # a list of package definitions, or
    cv.All(  # a single package definition (deprecated)
        cv.ensure_list(PACKAGE_SCHEMA), deprecate_single_package
    ),
)


def _process_remote_package(config: dict[str, Any]) -> dict[str, Any]:
    """Clone/update a git repo and load the YAML files listed in the package definition.

    Returns ``{"packages": {<filename>: <loaded_yaml>, ...}}`` so the caller
    can recurse into the loaded packages. Each loaded YAML node is tagged
    with any ``vars:`` from the file entry via :func:`yaml_util.add_context`.

    If loading fails after cloning, attempts a revert and retry in case
    a prior cached checkout is stale.
    """
    repo_dir, revert = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config[CONF_REFRESH],
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )
    files: list[dict[str, Any]] = []

    if base_path := config.get(CONF_PATH):
        repo_dir = repo_dir / base_path

    for file in config[CONF_FILES]:
        if isinstance(file, str):
            files.append({CONF_PATH: file, CONF_VARS: {}})
        else:
            files.append(file)

    def _load_package_yaml(yaml_file: Path, filename: str) -> dict:
        """Load a YAML file from a remote package, validating min_version."""
        try:
            new_yaml = yaml_util.load_yaml(yaml_file)
        except EsphomeError as e:
            raise cv.Invalid(
                f"{filename} is not a valid YAML file."
                f" Please check the file contents.\n{e}"
            ) from e
        esphome_config = new_yaml.get(CONF_ESPHOME) or {}
        min_version = esphome_config.get(CONF_MIN_VERSION)
        if min_version is not None and cv.Version.parse(min_version) > cv.Version.parse(
            ESPHOME_VERSION
        ):
            raise cv.Invalid(
                f"Current ESPHome Version is too old to use"
                f" this package: {ESPHOME_VERSION} < {min_version}"
            )
        return new_yaml

    def get_packages(files: list[dict[str, Any]]) -> dict:
        packages: dict[str, Any] = {}
        for idx, file in enumerate(files):
            filename = file[CONF_PATH]
            yaml_file: Path = repo_dir / filename
            if not yaml_file.is_file():
                raise cv.Invalid(
                    f"{filename} does not exist in repository",
                    path=[CONF_FILES, idx, CONF_PATH],
                )
            new_yaml = _load_package_yaml(yaml_file, filename)
            new_yaml = yaml_util.add_context(new_yaml, file.get(CONF_VARS))
            packages[f"{filename}{idx}"] = new_yaml
        return packages

    if revert is not None:
        # If loading fails, the cached checkout may be stale — revert and retry once.
        try:
            return {CONF_PACKAGES: get_packages(files)}
        except cv.Invalid:
            revert()
        try:
            return {CONF_PACKAGES: get_packages(files)}
        except cv.Invalid as err:
            raise cv.Invalid(f"Failed to load packages. {err}", path=err.path) from err

    return {CONF_PACKAGES: get_packages(files)}


def _walk_package_dict(
    packages: dict,
    callback: PackageCallback,
    context: ContextVars | None,
    path: yaml_util.DocumentPath,
) -> cv.Invalid | None:
    """Iterate a packages dict in reverse priority order, invoking callback on each entry.

    Returns ``None`` on success, or the first :class:`cv.Invalid` error if a callback fails.
    """
    for package_name, package_config in reversed(packages.items()):
        with cv.prepend_path(package_name):
            try:
                packages[package_name] = callback(
                    package_config, context, path + [package_name]
                )
            except cv.Invalid as err:
                return err
    return None


def _walk_package_list(
    packages: list,
    callback: PackageCallback,
    context: ContextVars | None,
    path: yaml_util.DocumentPath,
) -> None:
    """Iterate a packages list in reverse priority order, invoking callback on each entry."""
    for idx in reversed(range(len(packages))):
        with cv.prepend_path(idx):
            packages[idx] = callback(packages[idx], context, path + [idx])


def _walk_packages(
    config: dict,
    callback: PackageCallback,
    context: ContextVars | None = None,
    validate_deprecated: bool = True,
    path: yaml_util.DocumentPath | None = None,
) -> dict:
    """Walks the packages structure in priority order, invoking ``callback`` on each package definition found.

    This function only iterates over the immediate ``packages:`` entries in *config*.
    If packages may contain nested ``packages:`` keys, the *callback* is responsible
    for recursing by calling ``_walk_packages`` on the returned package config.
    """
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]
    packages_path = (path or []) + [CONF_PACKAGES]

    with cv.prepend_path(CONF_PACKAGES):
        if isinstance(packages, yaml_util.IncludeFile):
            # If the packages key is an IncludeFile, resolve it first before processing.
            packages = resolve_include(
                packages, packages_path, context, strict_undefined=False
            )
        if not isinstance(packages, (dict, list)):
            raise cv.Invalid(
                f"Packages must be a key to value mapping or list, got {type(packages)} instead"
            )

        if not isinstance(packages, dict):
            _walk_package_list(packages, callback, context, packages_path)
        elif (
            result := _walk_package_dict(packages, callback, context, packages_path)
        ) is not None:
            if not validate_deprecated or any(
                is_package_definition(v) for v in packages.values()
            ):
                raise result
            # Fallback: treat the dict as a single deprecated package.
            # This block can be removed once the single-package
            # deprecation period (2026.7.0) is over.
            config[CONF_PACKAGES] = [packages]
            return _walk_packages(
                deprecate_single_package(config), callback, context, path=path
            )

    config[CONF_PACKAGES] = packages
    return config


def _substitute_package_definition(
    package_config: dict | str,
    context_vars: ContextVars | None,
    path: yaml_util.DocumentPath | None = None,
) -> dict | str:
    """Substitute variables in a package definition string or remote package dict.

    Only substitutes strings and remote package dicts (URLs, refs, paths).
    Local package contents are left untouched — they will be substituted
    later during the main substitution pass.
    """

    def do_substitute(package_config: dict | str) -> dict | str:
        # Collect undefined-variable errors (rather than raising strict) so the
        # path walked through a remote-package dict is preserved and the user
        # sees which field (url / path / ref / ...) referenced the undefined
        # variable.
        errors: ErrList = []
        package_config = substitute(
            item=package_config,
            path=path or [],
            parent_context=context_vars or ContextVars(),
            strict_undefined=False,
            errors=errors,
        )
        raise_first_undefined(errors, "package definition")
        return package_config

    if isinstance(package_config, str):
        return do_substitute(package_config)

    if isinstance(package_config, dict) and is_remote_package(package_config):
        # Mark vars as literal to avoid substituting variables in the vars block itself, since they are meant to be
        # passed as-is to the package YAML and may contain their own substitution expressions that should not
        # be prematurely evaluated here.
        if CONF_FILES in package_config:
            for file_def in package_config[CONF_FILES]:
                if isinstance(file_def, dict) and CONF_VARS in file_def:
                    file_def[CONF_VARS] = yaml_util.make_literal(file_def[CONF_VARS])

        package_config = do_substitute(package_config)

    return package_config


def _update_substitutions_context(
    parent_context: UserDict,
    package_substitutions: dict[str, Any],
    eval_context: ContextVars | None = None,
) -> None:
    """Resolve and add new substitutions to the parent context.

    Skips keys already present (higher-priority sources win).
    String values are substituted against *eval_context* (or *parent_context*
    if not provided) so that cross-references between substitutions are
    expanded when possible. Resolved values are written into *parent_context*
    and back into *package_substitutions* so that subsequent merges into the
    consolidated ``substitutions:`` block carry the resolved value (the
    package's ``!include vars`` are no longer in scope after this function
    returns).

    *eval_context* may layer additional vars (e.g. a package's own ``!include
    vars``) on top of *parent_context* so that a package's substitutions can
    reference vars passed in by the parent file.
    """
    if eval_context is None:
        eval_context = ContextVars(parent_context)
    for key, value in package_substitutions.items():
        if key in parent_context:
            continue
        if not isinstance(value, str):
            parent_context[key] = value
            continue
        resolved = substitute(
            item=value,
            path=[CONF_SUBSTITUTIONS, key],
            parent_context=eval_context,
            strict_undefined=False,
        )
        parent_context[key] = resolved
        package_substitutions[key] = resolved


class _PackageProcessor:
    """Stateful processor that resolves packages and collects substitutions.

    Packages are processed highest-priority first (later-declared before
    earlier-declared) so that their substitutions are available when
    resolving lower-priority package definitions.  For each entry:

    1. Substitute variables in remote package definitions (URLs, refs, paths).
    2. Validate against ``PACKAGE_SCHEMA`` and download remote packages.
    3. Extract ``substitutions:`` and merge into the shared context
       (higher-priority packages win on conflicts).
    4. Recurse into any nested ``packages:`` keys.

    Command-line substitutions take the highest priority and are never overridden.
    """

    def __init__(
        self,
        substitutions: UserDict,
        command_line_substitutions: dict[str, Any] | None,
    ) -> None:
        self.substitutions = substitutions
        self.parent_context = UserDict(command_line_substitutions or {})

    def resolve_package(
        self,
        package_config: dict | str | yaml_util.IncludeFile,
        context_vars: ContextVars | None,
        path: yaml_util.DocumentPath,
    ) -> dict:
        """Resolve a package definition to a concrete ``dict`` and fetch remote packages.

        The input may be a ``str`` (git shorthand or Jinja expression), a
        ``dict`` (remote or local package), or an ``IncludeFile`` whose filename
        may itself contain substitution expressions.

        The loop handles the case where loading an ``IncludeFile`` yields another
        ``IncludeFile`` (e.g. a chain of deferred includes).  Each iteration:

        1. If the current value is an ``IncludeFile``, load it — resolving any
           substitutions in its filename first.
        2. Substitute variables in the resulting value (for strings and remote
           package dicts).
        3. Validate against ``PACKAGE_SCHEMA``.  If the result is a ``dict``,
           the loop exits; otherwise another iteration is needed.

        Raises ``cv.Invalid`` if the chain has not resolved to a ``dict`` after
        ``MAX_INCLUDE_DEPTH`` iterations.
        """
        for _ in range(MAX_INCLUDE_DEPTH):
            if isinstance(package_config, yaml_util.IncludeFile):
                package_config = resolve_include(
                    package_config,
                    path,
                    context_vars or ContextVars(),
                    strict_undefined=False,
                )

            package_config = _substitute_package_definition(
                package_config, context_vars, path
            )
            package_config = PACKAGE_SCHEMA(package_config)
            if isinstance(package_config, dict):
                break
        else:
            raise cv.Invalid(
                f"Maximum include nesting depth ({MAX_INCLUDE_DEPTH}) exceeded"
            )

        if is_remote_package(package_config):
            package_config = _process_remote_package(package_config)
        return package_config

    def collect_substitutions(
        self,
        package_config: dict,
        context_vars: ContextVars | None,
    ) -> ContextVars:
        """Extract substitutions from a package and merge into the shared context.

        Returns the context updated with the package's ``!include vars`` (or
        an equivalent of *context_vars* if the package has none) so the caller
        can reuse it when recursing into nested packages. ``None`` inputs are
        normalized to an empty :class:`ContextVars`, so the result is always
        non-``None``.
        """
        # Push the package's own !include vars before evaluating its
        # substitutions so they can reference vars passed in by the parent
        # (e.g. ``vars: {my_variable: ...}`` on the include entry).
        package_context = push_context(
            package_config, context_vars if context_vars is not None else ContextVars()
        )
        if subs := package_config.pop(CONF_SUBSTITUTIONS, {}):
            # Resolve before merging so that values referencing the package's
            # ``!include vars`` are baked into the consolidated substitutions
            # block; once we return, the package vars are no longer in scope.
            # ``package_context`` is a ChainMap whose chain already terminates
            # in ``self.parent_context`` (set up by ``do_packages_pass``), so
            # ``parent_context`` mutations from ``_update_substitutions_context``
            # remain visible to evaluation reads.
            _update_substitutions_context(self.parent_context, subs, package_context)
            self.substitutions.data = merge_config(subs, self.substitutions.data)
        return package_context

    def process_package(
        self,
        package_config: dict | str,
        context_vars: ContextVars | None,
        path: yaml_util.DocumentPath,
    ) -> dict:
        """Resolve a single package and recurse into any nested packages."""
        from_remote = isinstance(package_config, dict) and is_remote_package(
            package_config
        )
        package_config = self.resolve_package(package_config, context_vars, path)
        context_vars = self.collect_substitutions(package_config, context_vars)

        if CONF_PACKAGES not in package_config:
            return package_config

        # Push context from !include vars on the packages key (the package root
        # was already pushed in collect_substitutions above).
        context_vars = push_context(package_config[CONF_PACKAGES], context_vars)
        # Disable the deprecated single-package fallback for remote
        # packages.  _process_remote_package returns dicts with
        # already-resolved values that is_package_definition cannot
        # distinguish from config fragments, so the fallback would
        # always fire and mask real errors with wrong paths
        # (packages->0 instead of packages-><name>).
        return _walk_packages(
            package_config,
            self.process_package,
            context_vars,
            validate_deprecated=not from_remote,
            path=path,
        )


def do_packages_pass(
    config: dict[str, Any],
    *,
    command_line_substitutions: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Load, validate, and flatten all packages in the config.

    Returns the config with all packages loaded in-place (but not yet merged)
    and a consolidated ``substitutions:`` block restored at the front.
    """
    if CONF_PACKAGES not in config:
        return config

    with cv.prepend_path(CONF_SUBSTITUTIONS):
        substitutions = UserDict(
            resolve_substitutions_block(
                config.pop(CONF_SUBSTITUTIONS, {}), command_line_substitutions
            )
        )
    processor = _PackageProcessor(substitutions, command_line_substitutions)
    _update_substitutions_context(processor.parent_context, substitutions)

    context_vars = push_context(
        config[CONF_PACKAGES], ContextVars(processor.parent_context)
    )
    _walk_packages(config, processor.process_package, context_vars)

    if substitutions:
        config[CONF_SUBSTITUTIONS] = substitutions.data

    return config


def merge_packages(config: dict) -> dict:
    """Flatten the ``packages:`` tree into the main config.

    Collects every package (including nested ones) into a flat list in
    priority order, then merges them into *config* using :func:`merge_config`.
    Higher-priority packages (declared later) override lower-priority ones.

    The ``packages:`` key is removed from the returned config.
    Must be called after :func:`do_packages_pass` has resolved all packages.
    """
    if CONF_PACKAGES not in config:
        return config

    # Build flat list of all package configs to merge in priority order:
    merge_list: list[dict] = []

    def process_package_callback(
        package_config: dict,
        context: ContextVars | None,
        path: yaml_util.DocumentPath | None = None,
    ) -> dict:
        """This will be called for each package found in the config."""
        merge_list.append(package_config)
        return _walk_packages(package_config, process_package_callback, path=path)

    _walk_packages(config, process_package_callback, validate_deprecated=False)
    # Merge all packages into the main config:
    config = reduce(lambda new, old: merge_config(old, new), merge_list, config)
    del config[CONF_PACKAGES]
    return config


def resolve_packages(
    config: dict[str, Any],
    *,
    command_line_substitutions: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Load and merge ``packages:`` in one call; return the flattened config.

    Convenience wrapper around :func:`do_packages_pass` followed by
    :func:`merge_packages`. External tools that want the package-
    merged dict (without going through full schema validation via
    :func:`esphome.config.read_config`) get one stable seam to call
    instead of having to chain the two functions and stay in sync
    with the pipeline order.

    Note: the full :func:`esphome.config.validate_config` pipeline
    runs two extra passes around the merge that this wrapper
    deliberately skips:

    1. :func:`esphome.components.substitutions.do_substitution_pass`
       runs BETWEEN :func:`do_packages_pass` and
       :func:`merge_packages`, so ``${var}`` placeholders inside
       package content are NOT resolved here. Callers that need
       substitution should invoke ``do_substitution_pass``
       themselves between calls, or go through the full
       ``validate_config``.
    2. :func:`esphome.config.resolve_extend_remove` runs AFTER
       :func:`merge_packages`, so top-level ``!remove`` / ``!extend``
       markers are NOT applied here. A package-contributed block
       paired with a top-level ``key: !remove`` will still appear
       in the returned dict (the marker just sits next to it).

    The wrapper exists for the "what blocks did packages
    contribute?" question — metadata callers that just need to
    see merged top-level keys. It is NOT a stand-in for
    :func:`esphome.config.validate_config` and the two passes
    above are the reasons why.

    Used by:

    - ``esphome/device-builder`` — the new WebSocket dashboard
      backend reads device metadata (api / wifi / target-platform
      flags) off the merged config so packages contribute the same
      blocks the compiler sees, not just whatever sits at the top
      of the user's YAML. See
      https://github.com/esphome/device-builder/issues/288 for the
      bug this fixes.

    Returns *config* unchanged when ``packages:`` isn't present, so
    callers can apply this unconditionally without having to peek
    at the config first.
    """
    if CONF_PACKAGES not in config:
        return config
    config = do_packages_pass(
        config, command_line_substitutions=command_line_substitutions
    )
    return merge_packages(config)

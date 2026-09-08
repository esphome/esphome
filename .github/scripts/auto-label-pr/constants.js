// Constants and markers for PR auto-labeling
module.exports = {
  BOT_COMMENT_MARKER: '<!-- auto-label-pr-bot -->',
  CODEOWNERS_MARKER: '<!-- codeowners-request -->',
  TOO_BIG_MARKER: '<!-- too-big-request -->',
  DEPRECATED_COMPONENT_MARKER: '<!-- deprecated-component-request -->',
  ORG_FORK_MARKER: '<!-- maintainer-access-warning -->',

  MANAGED_LABELS: [
    'new-component',
    'new-platform',
    'new-target-platform',
    'merging-to-release',
    'merging-to-beta',
    'chained-pr',
    'stacked-pr',
    'core',
    'small-pr',
    'medium-pr',
    'dashboard',
    'github-actions',
    'by-code-owner',
    'has-tests',
    'needs-tests',
    'needs-docs',
    'needs-developer-docs',
    'needs-codeowners',
    'too-big',
    'labeller-recheck',
    'bugfix',
    'new-feature',
    'new-feature-developer',
    'breaking-change',
    'developer-breaking-change',
    'undocumented-api-change',
    'code-quality',
    'deprecated-component'
  ],

  DOCS_PR_PATTERNS: [
    /https:\/\/github\.com\/esphome\/esphome\.io\/pull\/\d+/,
    /esphome\/esphome\.io#\d+/,
    // Keep matching the old esphome-docs name during the transition period
    /https:\/\/github\.com\/esphome\/esphome-docs\/pull\/\d+/,
    /esphome\/esphome-docs#\d+/
  ],

  DEVELOPER_DOCS_PR_PATTERNS: [
    /https:\/\/github\.com\/esphome\/developers\.esphome\.io\/pull\/\d+/,
    /esphome\/developers\.esphome\.io#\d+/
  ],

  // Files whose developer-facing changes are documented via Python docstrings
  // only - developers.esphome.io has no reference page for them yet, so PRs
  // touching nothing but these files (and tests/) skip needs-developer-docs.
  DEV_DOCS_EXEMPT_FILES: [
    'esphome/config_validation.py'
  ]
};

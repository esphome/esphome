// Constants and markers for PR auto-labeling
module.exports = {
  BOT_COMMENT_MARKER: '<!-- auto-label-pr-bot -->',
  CODEOWNERS_MARKER: '<!-- codeowners-request -->',
  TOO_BIG_MARKER: '<!-- too-big-request -->',

  MANAGED_LABELS: [
    'new-component',
    'new-platform',
    'new-target-platform',
    'merging-to-release',
    'merging-to-beta',
    'chained-pr',
    'core',
    'small-pr',
    'dashboard',
    'github-actions',
    'by-code-owner',
    'has-tests',
    'needs-tests',
    'needs-docs',
    'needs-codeowners',
    'too-big',
    'labeller-recheck',
    'bugfix',
    'new-feature',
    'breaking-change',
    'developer-breaking-change',
    'code-quality',
  ],

  DOCS_PR_PATTERNS: [
    /https:\/\/github\.com\/esphome\/esphome-docs\/pull\/\d+/,
    /esphome\/esphome-docs#\d+/
  ]
};

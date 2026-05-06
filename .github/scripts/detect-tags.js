/**
 * Shared tag detection from changed file paths.
 * Used by pr-title-check and auto-label-pr workflows.
 */

const COMPONENT_REGEX = /^esphome\/components\/([^\/]+)\//;

/**
 * Detect component names from changed files.
 * @param {string[]} changedFiles - List of changed file paths
 * @returns {Set<string>} Set of component names
 */
function detectComponents(changedFiles) {
  const components = new Set();
  for (const file of changedFiles) {
    const match = file.match(COMPONENT_REGEX);
    if (match) {
      components.add(match[1]);
    }
  }
  return components;
}

/**
 * Detect if core files were changed.
 * Core files are in esphome/core/ or top-level esphome/ directory.
 * @param {string[]} changedFiles - List of changed file paths
 * @returns {boolean}
 */
function hasCoreChanges(changedFiles) {
  return changedFiles.some(file =>
    file.startsWith('esphome/core/') ||
    (file.startsWith('esphome/') && file.split('/').length === 2)
  );
}

/**
 * Detect if dashboard files were changed.
 * @param {string[]} changedFiles - List of changed file paths
 * @returns {boolean}
 */
function hasDashboardChanges(changedFiles) {
  return changedFiles.some(file =>
    file.startsWith('esphome/dashboard/') ||
    file.startsWith('esphome/components/dashboard_import/')
  );
}

/**
 * Detect if GitHub Actions files were changed.
 * @param {string[]} changedFiles - List of changed file paths
 * @returns {boolean}
 */
function hasGitHubActionsChanges(changedFiles) {
  return changedFiles.some(file =>
    file.startsWith('.github/workflows/')
  );
}

module.exports = {
  COMPONENT_REGEX,
  detectComponents,
  hasCoreChanges,
  hasDashboardChanges,
  hasGitHubActionsChanges,
};

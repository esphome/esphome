const { DOCS_PR_PATTERNS } = require('./constants');
const {
  COMPONENT_REGEX,
  detectComponents,
  hasCoreChanges,
  hasDashboardChanges,
  hasGitHubActionsChanges,
} = require('../detect-tags');
const { loadCodeowners, getEffectiveOwners } = require('../codeowners');

// Top-level `CONFIG_SCHEMA = ...` (assignment) or `CONFIG_SCHEMA: ConfigType = ...` (annotation).
// Ruff/Black enforce exactly one space around `=` and no space before `:`,
// so we can match strictly: `CONFIG_SCHEMA ` or `CONFIG_SCHEMA:`.
const CONFIG_SCHEMA_REGEX = /^CONFIG_SCHEMA[ :]/m;

// Fetch a file's contents from the PR head SHA via the GitHub API.
// The auto-label workflow runs on `pull_request_target`, which checks out the
// base branch — files added by the PR don't exist in the workspace, so we have
// to fetch them from the head SHA. Returns null if the file can't be fetched.
async function fetchPrFileContent(github, context, path) {
  try {
    const { owner, repo } = context.repo;
    const { data } = await github.rest.repos.getContent({
      owner,
      repo,
      path,
      ref: context.payload.pull_request.head.sha,
    });
    return Buffer.from(data.content, 'base64').toString('utf8');
  } catch (error) {
    console.log(`Failed to fetch ${path} from PR head:`, error.message);
    return null;
  }
}

// Strategy: Merge branch detection
async function detectMergeBranch(context) {
  const labels = new Set();
  const baseRef = context.payload.pull_request.base.ref;

  if (baseRef === 'release') {
    labels.add('merging-to-release');
  } else if (baseRef === 'beta') {
    labels.add('merging-to-beta');
  } else if (baseRef !== 'dev') {
    labels.add('chained-pr');
  }

  return labels;
}

// Strategy: Component and platform labeling
async function detectComponentPlatforms(changedFiles, apiData) {
  const labels = new Set();
  const targetPlatformRegex = new RegExp(`^esphome\/components\/(${apiData.targetPlatforms.join('|')})/`);

  for (const comp of detectComponents(changedFiles)) {
    labels.add(`component: ${comp}`);
  }

  for (const file of changedFiles) {
    const platformMatch = file.match(targetPlatformRegex);
    if (platformMatch) {
      labels.add(`platform: ${platformMatch[1]}`);
    }
  }

  return labels;
}

// Strategy: New component detection
async function detectNewComponents(github, context, prFiles) {
  const labels = new Set();
  let hasYamlLoadable = false;
  const addedFiles = prFiles.filter(file => file.status === 'added').map(file => file.filename);

  for (const file of addedFiles) {
    const componentMatch = file.match(/^esphome\/components\/([^\/]+)\/__init__\.py$/);
    if (!componentMatch) continue;

    labels.add('new-component');
    const content = await fetchPrFileContent(github, context, file);
    if (content === null) {
      // Safe default: assume YAML-loadable so needs-docs behaviour is unchanged on fetch failure
      hasYamlLoadable = true;
      continue;
    }
    if (content.includes('IS_TARGET_PLATFORM = True')) {
      labels.add('new-target-platform');
    }
    if (CONFIG_SCHEMA_REGEX.test(content)) {
      hasYamlLoadable = true;
    }
  }

  return { labels, hasYamlLoadable };
}

// Strategy: New platform detection
async function detectNewPlatforms(github, context, prFiles, apiData) {
  const labels = new Set();
  let hasYamlLoadable = false;
  const addedFiles = prFiles.filter(file => file.status === 'added').map(file => file.filename);

  const platformPathPatterns = [
    /^esphome\/components\/([^\/]+)\/([^\/]+)\.py$/,
    /^esphome\/components\/([^\/]+)\/([^\/]+)\/__init__\.py$/,
  ];

  for (const file of addedFiles) {
    for (const re of platformPathPatterns) {
      const match = file.match(re);
      if (!match) continue;
      const platform = match[2];
      if (!apiData.platformComponents.includes(platform)) break;

      labels.add('new-platform');
      const content = await fetchPrFileContent(github, context, file);
      if (content === null) {
        // Safe default: assume YAML-loadable so needs-docs behaviour is unchanged on fetch failure
        hasYamlLoadable = true;
      } else if (CONFIG_SCHEMA_REGEX.test(content)) {
        hasYamlLoadable = true;
      }
      break;
    }
  }

  return { labels, hasYamlLoadable };
}

// Strategy: Core files detection
async function detectCoreChanges(changedFiles) {
  const labels = new Set();
  if (hasCoreChanges(changedFiles)) {
    labels.add('core');
  }
  return labels;
}

// Strategy: PR size detection
async function detectPRSize(prFiles, totalAdditions, totalDeletions, totalChanges, isMegaPR, SMALL_PR_THRESHOLD, MEDIUM_PR_THRESHOLD, TOO_BIG_THRESHOLD) {
  const labels = new Set();

  if (totalChanges <= SMALL_PR_THRESHOLD) {
    labels.add('small-pr');
    return labels;
  }

  if (totalChanges <= MEDIUM_PR_THRESHOLD) {
    labels.add('medium-pr');
    return labels;
  }

  const testAdditions = prFiles
    .filter(file => file.filename.startsWith('tests/'))
    .reduce((sum, file) => sum + (file.additions || 0), 0);
  const testDeletions = prFiles
    .filter(file => file.filename.startsWith('tests/'))
    .reduce((sum, file) => sum + (file.deletions || 0), 0);

  const nonTestChanges = (totalAdditions - testAdditions) - (totalDeletions - testDeletions);

  // Don't add too-big if mega-pr label is already present
  if (nonTestChanges > TOO_BIG_THRESHOLD && !isMegaPR) {
    labels.add('too-big');
  }

  return labels;
}

// Strategy: Dashboard changes
async function detectDashboardChanges(changedFiles) {
  const labels = new Set();
  if (hasDashboardChanges(changedFiles)) {
    labels.add('dashboard');
  }
  return labels;
}

// Strategy: GitHub Actions changes
async function detectGitHubActionsChanges(changedFiles) {
  const labels = new Set();
  if (hasGitHubActionsChanges(changedFiles)) {
    labels.add('github-actions');
  }
  return labels;
}

// Strategy: Code owner detection
async function detectCodeOwner(github, context, changedFiles) {
  const labels = new Set();

  try {
    const codeownersPatterns = loadCodeowners();
    const prAuthor = context.payload.pull_request.user.login;

    // Check if PR author is a codeowner of any changed file
    const effective = getEffectiveOwners(changedFiles, codeownersPatterns);
    if (effective.users.has(prAuthor)) {
      labels.add('by-code-owner');
    }
  } catch (error) {
    console.log('Failed to read or parse CODEOWNERS file:', error.message);
  }

  return labels;
}

// Strategy: Test detection
async function detectTests(changedFiles) {
  const labels = new Set();
  const testFiles = changedFiles.filter(file => file.startsWith('tests/'));

  if (testFiles.length > 0) {
    labels.add('has-tests');
  }

  return labels;
}

// Strategy: PR Template Checkbox detection
async function detectPRTemplateCheckboxes(context) {
  const labels = new Set();
  const prBody = context.payload.pull_request.body || '';

  console.log('Checking PR template checkboxes...');

  // Check for checked checkboxes in the "Types of changes" section
  const checkboxPatterns = [
    { pattern: /- \[x\] Bugfix \(non-breaking change which fixes an issue\)/i, label: 'bugfix' },
    { pattern: /- \[x\] New feature \(non-breaking change which adds functionality\)/i, label: 'new-feature' },
    { pattern: /- \[x\] Breaking change \(fix or feature that would cause existing functionality to not work as expected\)/i, label: 'breaking-change' },
    { pattern: /- \[x\] Developer breaking change \(an API change that could break external components\)/i, label: 'developer-breaking-change' },
    { pattern: /- \[x\] Undocumented C\+\+ API change \(removal or change of undocumented public methods that lambda users may depend on\)/i, label: 'undocumented-api-change' },
    { pattern: /- \[x\] Code quality improvements to existing code or addition of tests/i, label: 'code-quality' }
  ];

  for (const { pattern, label } of checkboxPatterns) {
    if (pattern.test(prBody)) {
      console.log(`Found checked checkbox for: ${label}`);
      labels.add(label);
    }
  }

  return labels;
}

// Strategy: Deprecated component detection
async function detectDeprecatedComponents(github, context, changedFiles) {
  const labels = new Set();
  const deprecatedInfo = [];
  const { owner, repo } = context.repo;

  // Compile regex once for better performance
  const componentFileRegex = COMPONENT_REGEX;

  // Get files that are modified or added in components directory
  const componentFiles = changedFiles.filter(file => componentFileRegex.test(file));

  if (componentFiles.length === 0) {
    return { labels, deprecatedInfo };
  }

  // Extract unique component names using the same regex
  const components = new Set();
  for (const file of componentFiles) {
    const match = file.match(componentFileRegex);
    if (match) {
      components.add(match[1]);
    }
  }

  // Get base branch ref to check if deprecation already exists for the component
  // This prevents flagging a PR that simply adds deprecation
  const baseRef = context.payload.pull_request.base.ref;

  // Check each component's __init__.py for DEPRECATED_COMPONENT constant
  for (const component of components) {
    const initFile = `esphome/components/${component}/__init__.py`;
    try {
      // Fetch file content from base branch using GitHub API
      const { data: fileData } = await github.rest.repos.getContent({
        owner,
        repo,
        path: initFile,
        ref: baseRef
      });

      // Decode base64 content
      const content = Buffer.from(fileData.content, 'base64').toString('utf8');

      // Look for DEPRECATED_COMPONENT = "message" or DEPRECATED_COMPONENT = 'message'
      // Support single quotes, double quotes, and triple quotes (for multiline)
      const doubleQuoteMatch = content.match(/DEPRECATED_COMPONENT\s*=\s*"""([\s\S]*?)"""/s) ||
                              content.match(/DEPRECATED_COMPONENT\s*=\s*"((?:[^"\\]|\\.)*)"/);
      const singleQuoteMatch = content.match(/DEPRECATED_COMPONENT\s*=\s*'''([\s\S]*?)'''/s) ||
                              content.match(/DEPRECATED_COMPONENT\s*=\s*'((?:[^'\\]|\\.)*)'/);
      const deprecatedMatch = doubleQuoteMatch || singleQuoteMatch;

      if (deprecatedMatch) {
        labels.add('deprecated-component');
        deprecatedInfo.push({
          component: component,
          message: deprecatedMatch[1].trim()
        });
        console.log(`Found deprecated component: ${component}`);
      }
    } catch (error) {
      // Only log if it's not a simple "file not found" error (404)
      if (error.status !== 404) {
        console.log(`Error reading ${initFile}:`, error.message);
      }
    }
  }

  return { labels, deprecatedInfo };
}

// Strategy: Detect when maintainers cannot modify the PR branch
function detectMaintainerAccess(context) {
  const pr = context.payload.pull_request;

  // Only relevant for cross-repo PRs (forks)
  if (!pr.head.repo || pr.head.repo.full_name === pr.base.repo.full_name) {
    return null;
  }

  if (pr.maintainer_can_modify) {
    return null;
  }

  const isOrgFork = pr.head.repo.owner.type === 'Organization';
  console.log(`Maintainer cannot modify PR branch (${isOrgFork ? 'org fork: ' + pr.head.repo.owner.login : 'user disabled'})`);
  return { isOrgFork, orgName: pr.head.repo.owner.login };
}

// Strategy: Requirements detection
async function detectRequirements(allLabels, prFiles, context, hasYamlLoadable) {
  const labels = new Set();

  // Check for missing tests
  if ((allLabels.has('new-component') || allLabels.has('new-platform') || allLabels.has('new-feature')) && !allLabels.has('has-tests')) {
    labels.add('needs-tests');
  }

  // Check for missing docs.
  // `new-feature` (PR-body checkbox) always counts. `new-component` / `new-platform`
  // only count when at least one newly added file defines a top-level CONFIG_SCHEMA,
  // i.e. the new component/platform is actually loadable from YAML.
  const docsEligible =
    allLabels.has('new-feature') ||
    ((allLabels.has('new-component') || allLabels.has('new-platform')) && hasYamlLoadable);

  if (docsEligible) {
    const prBody = context.payload.pull_request.body || '';
    const hasDocsLink = DOCS_PR_PATTERNS.some(pattern => pattern.test(prBody));

    if (!hasDocsLink) {
      labels.add('needs-docs');
    }
  }

  // Check for missing CODEOWNERS
  if (allLabels.has('new-component')) {
    const codeownersModified = prFiles.some(file =>
      file.filename === 'CODEOWNERS' &&
      (file.status === 'modified' || file.status === 'added') &&
      (file.additions || 0) > 0
    );

    if (!codeownersModified) {
      labels.add('needs-codeowners');
    }
  }

  return labels;
}

module.exports = {
  detectMergeBranch,
  detectComponentPlatforms,
  detectNewComponents,
  detectNewPlatforms,
  detectCoreChanges,
  detectPRSize,
  detectDashboardChanges,
  detectGitHubActionsChanges,
  detectCodeOwner,
  detectTests,
  detectPRTemplateCheckboxes,
  detectDeprecatedComponents,
  detectMaintainerAccess,
  detectRequirements
};

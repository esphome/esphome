const fs = require('fs');
const { DOCS_PR_PATTERNS } = require('./constants');

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
  const componentRegex = /^esphome\/components\/([^\/]+)\//;
  const targetPlatformRegex = new RegExp(`^esphome\/components\/(${apiData.targetPlatforms.join('|')})/`);

  for (const file of changedFiles) {
    const componentMatch = file.match(componentRegex);
    if (componentMatch) {
      labels.add(`component: ${componentMatch[1]}`);
    }

    const platformMatch = file.match(targetPlatformRegex);
    if (platformMatch) {
      labels.add(`platform: ${platformMatch[1]}`);
    }
  }

  return labels;
}

// Strategy: New component detection
async function detectNewComponents(prFiles) {
  const labels = new Set();
  const addedFiles = prFiles.filter(file => file.status === 'added').map(file => file.filename);

  for (const file of addedFiles) {
    const componentMatch = file.match(/^esphome\/components\/([^\/]+)\/__init__\.py$/);
    if (componentMatch) {
      try {
        const content = fs.readFileSync(file, 'utf8');
        if (content.includes('IS_TARGET_PLATFORM = True')) {
          labels.add('new-target-platform');
        }
      } catch (error) {
        console.log(`Failed to read content of ${file}:`, error.message);
      }
      labels.add('new-component');
    }
  }

  return labels;
}

// Strategy: New platform detection
async function detectNewPlatforms(prFiles, apiData) {
  const labels = new Set();
  const addedFiles = prFiles.filter(file => file.status === 'added').map(file => file.filename);

  for (const file of addedFiles) {
    const platformFileMatch = file.match(/^esphome\/components\/([^\/]+)\/([^\/]+)\.py$/);
    if (platformFileMatch) {
      const [, component, platform] = platformFileMatch;
      if (apiData.platformComponents.includes(platform)) {
        labels.add('new-platform');
      }
    }

    const platformDirMatch = file.match(/^esphome\/components\/([^\/]+)\/([^\/]+)\/__init__\.py$/);
    if (platformDirMatch) {
      const [, component, platform] = platformDirMatch;
      if (apiData.platformComponents.includes(platform)) {
        labels.add('new-platform');
      }
    }
  }

  return labels;
}

// Strategy: Core files detection
async function detectCoreChanges(changedFiles) {
  const labels = new Set();
  const coreFiles = changedFiles.filter(file =>
    file.startsWith('esphome/core/') ||
    (file.startsWith('esphome/') && file.split('/').length === 2)
  );

  if (coreFiles.length > 0) {
    labels.add('core');
  }

  return labels;
}

// Strategy: PR size detection
async function detectPRSize(prFiles, totalAdditions, totalDeletions, totalChanges, isMegaPR, SMALL_PR_THRESHOLD, TOO_BIG_THRESHOLD) {
  const labels = new Set();

  if (totalChanges <= SMALL_PR_THRESHOLD) {
    labels.add('small-pr');
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
  const dashboardFiles = changedFiles.filter(file =>
    file.startsWith('esphome/dashboard/') ||
    file.startsWith('esphome/components/dashboard_import/')
  );

  if (dashboardFiles.length > 0) {
    labels.add('dashboard');
  }

  return labels;
}

// Strategy: GitHub Actions changes
async function detectGitHubActionsChanges(changedFiles) {
  const labels = new Set();
  const githubActionsFiles = changedFiles.filter(file =>
    file.startsWith('.github/workflows/')
  );

  if (githubActionsFiles.length > 0) {
    labels.add('github-actions');
  }

  return labels;
}

// Strategy: Code owner detection
async function detectCodeOwner(github, context, changedFiles) {
  const labels = new Set();
  const { owner, repo } = context.repo;

  try {
    const { data: codeownersFile } = await github.rest.repos.getContent({
      owner,
      repo,
      path: 'CODEOWNERS',
    });

    const codeownersContent = Buffer.from(codeownersFile.content, 'base64').toString('utf8');
    const prAuthor = context.payload.pull_request.user.login;

    const codeownersLines = codeownersContent.split('\n')
      .map(line => line.trim())
      .filter(line => line && !line.startsWith('#'));

    const codeownersRegexes = codeownersLines.map(line => {
      const parts = line.split(/\s+/);
      const pattern = parts[0];
      const owners = parts.slice(1);

      let regex;
      if (pattern.endsWith('*')) {
        const dir = pattern.slice(0, -1);
        regex = new RegExp(`^${dir.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}`);
      } else if (pattern.includes('*')) {
        // First escape all regex special chars except *, then replace * with .*
        const regexPattern = pattern
          .replace(/[.+?^${}()|[\]\\]/g, '\\$&')
          .replace(/\*/g, '.*');
        regex = new RegExp(`^${regexPattern}$`);
      } else {
        regex = new RegExp(`^${pattern.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}$`);
      }

      return { regex, owners };
    });

    for (const file of changedFiles) {
      for (const { regex, owners } of codeownersRegexes) {
        if (regex.test(file) && owners.some(owner => owner === `@${prAuthor}`)) {
          labels.add('by-code-owner');
          return labels;
        }
      }
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

// Strategy: Requirements detection
async function detectRequirements(allLabels, prFiles, context) {
  const labels = new Set();

  // Check for missing tests
  if ((allLabels.has('new-component') || allLabels.has('new-platform') || allLabels.has('new-feature')) && !allLabels.has('has-tests')) {
    labels.add('needs-tests');
  }

  // Check for missing docs
  if (allLabels.has('new-component') || allLabels.has('new-platform') || allLabels.has('new-feature')) {
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
  detectRequirements
};

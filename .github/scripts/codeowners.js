// Shared CODEOWNERS parsing and matching utilities.
//
// Used by:
//   - codeowner-review-request.yml
//   - codeowner-approved-label-update.yml
//   - auto-label-pr/detectors.js (detectCodeOwner)

/**
 * Convert a CODEOWNERS glob pattern to a RegExp.
 *
 * Handles **, *, and ? wildcards after escaping regex-special characters.
 */
function globToRegex(pattern) {
  let regexStr = pattern
    .replace(/([.+^=!:${}()|[\]\\])/g, '\\$1')
    .replace(/\*\*/g, '\x00GLOBSTAR\x00')  // protect ** from next replace
    .replace(/\*/g, '[^/]*')               // single star
    .replace(/\x00GLOBSTAR\x00/g, '.*')    // restore globstar
    .replace(/\?/g, '.');
  return new RegExp('^' + regexStr + '$');
}

/**
 * Parse raw CODEOWNERS file content into an array of
 * { pattern, regex, owners } objects.
 *
 * Each `owners` entry is the raw string from the file (e.g. "@user" or
 * "@esphome/core").
 */
function parseCodeowners(content) {
  const lines = content
    .split('\n')
    .map(line => line.trim())
    .filter(line => line && !line.startsWith('#'));

  const patterns = [];
  for (const line of lines) {
    const parts = line.split(/\s+/);
    if (parts.length < 2) continue;

    const pattern = parts[0];
    const owners = parts.slice(1);
    const regex = globToRegex(pattern);
    patterns.push({ pattern, regex, owners });
  }
  return patterns;
}

/**
 * Fetch and parse the CODEOWNERS file via the GitHub API.
 *
 * @param {object} github  - octokit instance from actions/github-script
 * @param {string} owner   - repo owner
 * @param {string} repo    - repo name
 * @param {string} [ref]   - git ref (SHA / branch) to read from
 * @returns {Array<{pattern: string, regex: RegExp, owners: string[]}>}
 */
async function fetchCodeowners(github, owner, repo, ref) {
  const params = { owner, repo, path: 'CODEOWNERS' };
  if (ref) params.ref = ref;

  const { data: file } = await github.rest.repos.getContent(params);
  const content = Buffer.from(file.content, 'base64').toString('utf8');
  return parseCodeowners(content);
}

/**
 * Classify raw owner strings into individual users and teams.
 *
 * @param {string[]} rawOwners - e.g. ["@user1", "@esphome/core"]
 * @returns {{ users: string[], teams: string[] }}
 *   users  – login names without "@"
 *   teams  – team slugs without the "org/" prefix
 */
function classifyOwners(rawOwners) {
  const users = [];
  const teams = [];
  for (const o of rawOwners) {
    const clean = o.startsWith('@') ? o.slice(1) : o;
    if (clean.includes('/')) {
      teams.push(clean.split('/')[1]);
    } else {
      users.push(clean);
    }
  }
  return { users, teams };
}

/**
 * For each file, find its effective codeowners using GitHub's
 * "last match wins" semantics, then union across all files.
 *
 * @param {string[]} files              - list of file paths
 * @param {Array}    codeownersPatterns  - from parseCodeowners / fetchCodeowners
 * @returns {{ users: Set<string>, teams: Set<string>, matchedFileCount: number }}
 */
function getEffectiveOwners(files, codeownersPatterns) {
  const users = new Set();
  const teams = new Set();
  let matchedFileCount = 0;

  for (const file of files) {
    // Last matching pattern wins for each file
    let effectiveOwners = null;
    for (const { regex, owners } of codeownersPatterns) {
      if (regex.test(file)) {
        effectiveOwners = owners;
      }
    }
    if (effectiveOwners) {
      matchedFileCount++;
      const classified = classifyOwners(effectiveOwners);
      for (const u of classified.users) users.add(u);
      for (const t of classified.teams) teams.add(t);
    }
  }

  return { users, teams, matchedFileCount };
}

/**
 * Read and parse the CODEOWNERS file from disk.
 *
 * Use this when the repo is already checked out (avoids an API call).
 *
 * @param {string} [repoRoot='.'] - path to the repo root
 * @returns {Array<{pattern: string, regex: RegExp, owners: string[]}>}
 */
function loadCodeowners(repoRoot = '.') {
  const fs = require('fs');
  const path = require('path');
  const content = fs.readFileSync(path.join(repoRoot, 'CODEOWNERS'), 'utf8');
  return parseCodeowners(content);
}

/** Possible label actions returned by determineLabelAction. */
const LabelAction = Object.freeze({
  ADD: 'add',
  REMOVE: 'remove',
  NONE: 'none',
});

/**
 * Determine what label action is needed for a PR based on codeowner approvals.
 *
 * Checks changed files against CODEOWNERS patterns, reviews, and current labels
 * to decide if the label should be added, removed, or left unchanged.
 *
 * @param {object} github              - octokit instance from actions/github-script
 * @param {string} owner               - repo owner
 * @param {string} repo                - repo name
 * @param {number} pr_number           - pull request number
 * @param {Array}  codeownersPatterns   - from loadCodeowners / fetchCodeowners
 * @param {string} labelName           - label to manage
 * @returns {Promise<LabelAction>}
 */
async function determineLabelAction(github, owner, repo, pr_number, codeownersPatterns, labelName) {
  // Get the list of changed files in this PR
  const prFiles = await github.paginate(
    github.rest.pulls.listFiles,
    { owner, repo, pull_number: pr_number }
  );

  const changedFiles = prFiles.map(file => file.filename);
  console.log(`Found ${changedFiles.length} changed files`);

  if (changedFiles.length === 0) {
    console.log('No changed files found');
    return LabelAction.NONE;
  }

  // Get effective owners using last-match-wins semantics
  const effective = getEffectiveOwners(changedFiles, codeownersPatterns);
  const componentCodeowners = effective.users;

  console.log(`Component-specific codeowners: ${Array.from(componentCodeowners).join(', ') || '(none)'}`);

  // Get current labels
  const { data: currentLabels } = await github.rest.issues.listLabelsOnIssue({
    owner, repo, issue_number: pr_number
  });
  const hasLabel = currentLabels.some(label => label.name === labelName);

  if (componentCodeowners.size === 0) {
    console.log('No component-specific codeowners found');
    return hasLabel ? LabelAction.REMOVE : LabelAction.NONE;
  }

  // Get all reviews and find latest per user
  const reviews = await github.paginate(
    github.rest.pulls.listReviews,
    { owner, repo, pull_number: pr_number }
  );

  const latestReviewByUser = new Map();
  for (const review of reviews) {
    if (!review.user || review.user.type === 'Bot' || review.state === 'COMMENTED') continue;
    latestReviewByUser.set(review.user.login, review);
  }

  // Check if any component-specific codeowner has an active approval
  let hasCodeownerApproval = false;
  for (const [login, review] of latestReviewByUser) {
    if (review.state === 'APPROVED' && componentCodeowners.has(login)) {
      console.log(`Codeowner '${login}' has approved`);
      hasCodeownerApproval = true;
      break;
    }
  }

  if (hasCodeownerApproval && !hasLabel) return LabelAction.ADD;
  if (!hasCodeownerApproval && hasLabel) return LabelAction.REMOVE;

  console.log(`Label already ${hasLabel ? 'present' : 'absent'}, no change needed`);
  return LabelAction.NONE;
}

module.exports = {
  globToRegex,
  parseCodeowners,
  fetchCodeowners,
  loadCodeowners,
  classifyOwners,
  getEffectiveOwners,
  LabelAction,
  determineLabelAction
};

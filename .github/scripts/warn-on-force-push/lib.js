// Core logic for the "warn on history-rewriting force-push" workflow, split
// out from index.js so it can be unit tested without a live GitHub API.

// GitHub Apps report `type: "Bot"`, but this repo's PAT-backed bots
// (esphbot, bluetoothbot, esphomebot) report `type: "User"` and need an
// explicit login check too.
const BOT_LOGINS = new Set(['esphbot', 'bluetoothbot', 'esphomebot']);

const HUMAN_REVIEW_STATES = ['COMMENTED', 'CHANGES_REQUESTED', 'APPROVED', 'DISMISSED'];

function isBot(user) {
  return user?.type === 'Bot' || BOT_LOGINS.has(user?.login?.toLowerCase());
}

// GitHub's "suggest a change" comments embed a ```suggestion fenced block.
// Restricting inline-comment counting to these (rather than any inline
// comment) means a plain self-note left by the PR author on their own diff
// doesn't count as review activity.
function isCodeSuggestion(body) {
  return typeof body === 'string' && /```suggestion\b/.test(body);
}

// compareCommits 404s if either `before` or `after` can't be resolved. Only
// call it a rewrite if `after` resolves and `before` doesn't -- otherwise
// (e.g. a fork was deleted or made private) stay silent rather than risk a
// false positive.
async function isRewrite(github, owner, repo, before, after, core) {
  let comparison;
  try {
    ({ data: comparison } = await github.rest.repos.compareCommits({
      owner,
      repo,
      base: before,
      head: after,
    }));
  } catch (err) {
    if (err.status !== 404) {
      throw err;
    }
    try {
      await github.rest.repos.getCommit({ owner, repo, ref: after });
    } catch {
      core.info(`Could not resolve ${after} — skipping.`);
      return false;
    }
    core.info(`Previous tip ${before} is no longer reachable — history was rewritten.`);
    return true;
  }
  // Fast-forward: `after` descends from `before` (status "ahead", behind_by
  // 0). Anything else -- diverged, or `before` ahead of `after` -- is a
  // rewrite.
  return comparison.status === 'diverged' || comparison.behind_by > 0;
}

// "Review has started" means either a human left an inline code-change
// suggestion, or submitted a top-level review with a countable state.
function hasHumanReview(reviewComments, reviews) {
  return (
    reviewComments.some(c => !isBot(c.user) && isCodeSuggestion(c.body)) ||
    reviews.some(r => !isBot(r.user) && HUMAN_REVIEW_STATES.includes(r.state))
  );
}

// Git ref names allow backticks, @, and other characters that could break
// markdown formatting or ping an uninvolved user when embedded in prose.
function sanitizeForProse(branch) {
  return branch.replace(/`/g, '').replace(/@/g, '@​');
}

// `branch` (unsanitized) is used only inside the fenced restore command:
// GitHub renders neither @-mentions nor inline markdown inside a fenced code
// block, and the command must stay copy-pasteable as-is.
function buildWarningBody({ marker, branch, before, after, owner, repo }) {
  const safeBranch = sanitizeForProse(branch);
  return [
    marker,
    "### ⚠️ This branch's history was rewritten",
    '',
    `A force-push to \`${safeBranch}\` replaced earlier commits — the previous tip is no longer an ancestor of the current tip (\`${after.slice(0, 7)}\`).`,
    '',
    'Rewriting history mid-review makes it harder to track what changed since the last look, and can detach existing review comments from their code. Please avoid `git push --force` on PR branches — push new commits on top instead, or check with reviewers first if a rebase is really needed.',
    '',
    "**To restore the previous state, if this wasn't intentional:**",
    '',
    'From a local clone that still has the old commit, push the previous tip back onto the branch:',
    '```',
    `git push --force origin ${before}:${branch}`,
    '```',
    `Or view it first at https://github.com/${owner}/${repo}/commit/${before}.`,
    '',
    "That commit may no longer be fetchable with `git fetch` once no branch or tag references it, but it's often still viewable at the link above for a while.",
  ].join('\n');
}

module.exports = {
  isBot,
  isRewrite,
  hasHumanReview,
  sanitizeForProse,
  buildWarningBody,
};

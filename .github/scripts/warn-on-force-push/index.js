const { isRewrite, hasHumanReview, sanitizeForProse, buildWarningBody } = require('./lib');

const MARKER = '<!-- force-push-warning -->';

module.exports = async ({ github, context, core }) => {
  const branch = context.payload.pull_request.head.ref;
  const prNumber = context.payload.pull_request.number;
  const { owner, repo } = context.repo;
  const before = process.env.BEFORE;
  const after = process.env.AFTER;

  if (!(await isRewrite(github, owner, repo, before, after, core))) {
    core.info('Fast-forward update — nothing to do.');
    return;
  }

  const comments = await github.paginate(github.rest.issues.listComments, {
    owner,
    repo,
    issue_number: prNumber,
    per_page: 100,
  });
  if (comments.some(c => c.body.includes(MARKER))) {
    core.info('Warning already posted for this PR — suppressing.');
    return;
  }

  const reviewComments = await github.paginate(github.rest.pulls.listReviewComments, {
    owner,
    repo,
    pull_number: prNumber,
    per_page: 100,
  });
  const reviews = await github.paginate(github.rest.pulls.listReviews, {
    owner,
    repo,
    pull_number: prNumber,
    per_page: 100,
  });
  if (!hasHumanReview(reviewComments, reviews)) {
    core.info('No existing human review activity — skipping warning.');
    return;
  }

  const safeBranch = sanitizeForProse(branch);
  const body = buildWarningBody({ marker: MARKER, branch, safeBranch, before, after, owner, repo });

  await github.rest.issues.createComment({
    owner,
    repo,
    issue_number: prNumber,
    body,
  });
};

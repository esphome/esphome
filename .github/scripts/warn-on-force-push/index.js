const { isRewrite, hasHumanReview, buildWarningBody } = require('./lib');

const MARKER = '<!-- force-push-warning -->';

module.exports = async ({ github, context, core }) => {
  const branch = context.payload.pull_request.head.ref;
  const prNumber = context.payload.pull_request.number;
  const prAuthorLogin = context.payload.pull_request.user.login;
  const { owner, repo } = context.repo;
  const before = process.env.BEFORE;
  const after = process.env.AFTER;

  if (!(await isRewrite(github, owner, repo, before, after, core))) {
    core.info('Fast-forward update — nothing to do.');
    return;
  }

  let alreadyPosted = false;
  for await (const { data: page } of github.paginate.iterator(github.rest.issues.listComments, {
    owner,
    repo,
    issue_number: prNumber,
    per_page: 100,
  })) {
    if (page.some(c => c.body?.includes(MARKER))) {
      alreadyPosted = true;
      break;
    }
  }
  if (alreadyPosted) {
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
  if (!hasHumanReview(reviewComments, reviews, prAuthorLogin)) {
    core.info('No existing human review activity — skipping warning.');
    return;
  }

  const body = buildWarningBody({ marker: MARKER, branch, before, after, owner, repo });

  await github.rest.issues.createComment({
    owner,
    repo,
    issue_number: prNumber,
    body,
  });
};

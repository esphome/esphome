// Run by .github/workflows/rerun-failed-ci-jobs.yml for a `/rerun-failed` comment.
module.exports = async ({ github, context, core }) => {
  const { owner, repo } = context.repo;
  const { comment, issue } = context.payload;
  const MAX_RERUNS = 2;

  if (!issue.pull_request || comment.body.trim() !== '/rerun-failed') {
    core.info('Not a /rerun-failed command');
    return;
  }

  // The reaction is only the answer, so a failed one is logged and never fails the step.
  const react = async (content) => {
    try {
      await github.rest.reactions.createForIssueComment({ owner, repo, comment_id: comment.id, content });
    } catch (error) {
      core.info(`Could not add the ${content} reaction: ${error.message}`);
    }
  };

  // The PR author may always use the command; anyone else needs write access.
  const login = comment.user.login;
  let allowed = login === issue.user.login;
  if (!allowed) {
    try {
      const { data } = await github.rest.repos.getCollaboratorPermissionLevel({
        owner, repo, username: login,
      });
      allowed = ['admin', 'maintain', 'write'].some((p) => p === data.permission || p === data.role_name);
    } catch (error) {
      core.info(`Could not read the permission of ${login}: ${error.message}`);
    }
  }
  if (!allowed) {
    core.info(`${login} is not the PR author and has no write access`);
    await react('confused');
    return;
  }

  // The newest CI run for the PR's head commit. The head comes from the
  // API, never from the comment, and the API lists the newest run first.
  const { data: pr } = await github.rest.pulls.get({ owner, repo, pull_number: issue.number });
  const runs = await github.paginate(github.rest.actions.listWorkflowRuns, {
    owner, repo, workflow_id: 'ci.yml', event: 'pull_request', head_sha: pr.head.sha, per_page: 100,
  });
  // Another PR can have the same head commit, and fork runs list no pull_requests.
  const run = runs.find((candidate) =>
    candidate.head_repository?.id === pr.head.repo?.id && candidate.head_branch === pr.head.ref
  );
  // No run says which PR it is for. While another PR from the same branch is open,
  // or was closed after this run was made, the run can be that PR's.
  if (run) {
    const { data: branchPrs } = await github.rest.pulls.list({
      owner, repo, state: 'all', head: pr.head.label, per_page: 100,
    });
    const shared = branchPrs.some((other) => other.number !== pr.number &&
      (other.state === 'open' || Date.parse(other.closed_at) > Date.parse(run.created_at)));
    if (shared) {
      core.info(`CI run ${run.id} can belong to another PR from ${pr.head.label}`);
      await react('confused');
      return;
    }
  }
  if (run && run.status !== 'completed') {
    core.info(`CI run ${run.id} for ${pr.head.sha} is still ${run.status}`);
    await react('eyes');
    return;
  }
  // Cancelled runs and runs waiting for approval are left to the maintainers.
  if (!run || run.conclusion !== 'failure') {
    core.info(`No failed CI run for ${pr.head.sha}`);
    await react('confused');
    return;
  }
  // run_attempt is 1 for the original run.
  if (run.run_attempt - 1 >= MAX_RERUNS) {
    core.info(`The failed jobs of CI run ${run.id} were already re-run ${MAX_RERUNS} times`);
    await react('confused');
    return;
  }

  try {
    await github.rest.actions.reRunWorkflowFailedJobs({ owner, repo, run_id: run.id });
  } catch (error) {
    await react('confused');
    // A 403 means GitHub refuses to re-run this run, such as one older than 30 days.
    // The step still passes: a failed one would be a red check on the default branch.
    if (error.status === 403) {
      core.info(`GitHub refused to re-run the failed jobs of CI run ${run.id}: ${error.message}`);
      return;
    }
    throw error;
  }
  core.info(`Re-running the failed jobs of CI run ${run.id}`);
  await react('rocket');
};

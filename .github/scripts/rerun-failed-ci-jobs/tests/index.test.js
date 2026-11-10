const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const rerunFailed = require('../index');

// The PR under test: number 4242, opened by "author". Its head commit is HEAD_SHA.
const PR_NUMBER = 4242;
const AUTHOR = 'author';
const HEAD_SHA = '2'.repeat(40);
const OLD_SHA = '1'.repeat(40);

// One run of ci.yml. By default the finished, failed first attempt of the
// PR's head commit.
function ciRun({ id, sha = HEAD_SHA, status = 'completed', conclusion = 'failure', attempt = 1 }) {
  return {
    id,
    path: '.github/workflows/ci.yml',
    event: 'pull_request',
    head_sha: sha,
    status,
    conclusion: status === 'completed' ? conclusion : null,
    run_attempt: attempt,
  };
}

function httpError(status, message) {
  return Object.assign(new Error(message), { status });
}

// A fake of the GitHub API calls the script makes; it records every call.
// `runs` are listed newest first, as the API does. `permissions` maps a login
// to the answer of the permission API; any other login gets a 404, like a user
// the API does not know. `rerunError` and `reactionError` make that call fail.
function fakeGitHub({ runs = [], permissions = {}, rerunError = null, reactionError = null } = {}) {
  const calls = [];
  const github = {
    rest: {
      actions: {
        // Like the API: only runs of the given workflow file, event and head commit.
        listWorkflowRuns: async (params) => {
          calls.push(['listWorkflowRuns', params]);
          const found = runs.filter((run) =>
            run.path === `.github/workflows/${params.workflow_id}` &&
            run.event === params.event &&
            run.head_sha === params.head_sha
          );
          return { data: { total_count: found.length, workflow_runs: found.slice(0, params.per_page) } };
        },
        reRunWorkflowFailedJobs: async (params) => {
          calls.push(['rerun', params.run_id]);
          if (rerunError) {
            throw rerunError;
          }
        },
      },
      pulls: {
        get: async (params) => {
          calls.push(['pulls.get', params.pull_number]);
          return { data: { number: PR_NUMBER, head: { sha: HEAD_SHA } } };
        },
      },
      reactions: {
        createForIssueComment: async (params) => {
          calls.push(['reaction', params.content]);
          if (reactionError) {
            throw reactionError;
          }
        },
      },
      repos: {
        getCollaboratorPermissionLevel: async (params) => {
          calls.push(['permission', params.username]);
          if (!(params.username in permissions)) {
            throw httpError(404, 'Not Found');
          }
          return { data: permissions[params.username] };
        },
      },
    },
  };
  return { github, calls };
}

// The issue_comment event: `commenter` writes `body` on the PR, or on an issue.
function commentEvent({ body = '/rerun-failed', commenter = AUTHOR, onIssue = false } = {}) {
  const issue = { number: PR_NUMBER, state: 'open', user: { login: AUTHOR } };
  if (!onIssue) {
    issue.pull_request = { url: `https://api.github.com/repos/esphome/esphome/pulls/${PR_NUMBER}` };
  }
  return {
    repo: { owner: 'esphome', repo: 'esphome' },
    payload: { comment: { id: 77, body, user: { login: commenter } }, issue },
  };
}

// Runs the script the way the workflow does and returns its log lines.
async function runCommand(event, api) {
  const log = [];
  await rerunFailed({ github: api.github, context: event, core: { info: (message) => log.push(message) } });
  return log;
}

const reruns = (calls) => calls.filter(([name]) => name === 'rerun').map(([, runId]) => runId);
const reactions = (calls) => calls.filter(([name]) => name === 'reaction').map(([, content]) => content);

describe('who may use the command', () => {
  it('lets the PR author re-run the failed jobs of a red CI run', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), [1]);
    assert.deepEqual(reactions(api.calls), ['rocket']);
  });

  it('does not ask for the permission of the PR author', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent(), api);
    assert.equal(api.calls.some(([name]) => name === 'permission'), false);
  });

  it('lets a maintainer use it on a PR of someone else', async () => {
    const api = fakeGitHub({
      runs: [ciRun({ id: 1 })],
      permissions: { maintainer: { permission: 'write', role_name: 'maintain' } },
    });
    await runCommand(commentEvent({ commenter: 'maintainer' }), api);
    assert.deepEqual(reruns(api.calls), [1]);
    assert.deepEqual(reactions(api.calls), ['rocket']);
  });

  it('refuses a user with read access before looking at the PR', async () => {
    const api = fakeGitHub({
      runs: [ciRun({ id: 1 })],
      permissions: { stranger: { permission: 'read', role_name: 'read' } },
    });
    await runCommand(commentEvent({ commenter: 'stranger' }), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
    assert.equal(api.calls.some(([name]) => name === 'pulls.get'), false);
  });

  it('refuses triage access', async () => {
    const api = fakeGitHub({
      runs: [ciRun({ id: 1 })],
      permissions: { helper: { permission: 'read', role_name: 'triage' } },
    });
    await runCommand(commentEvent({ commenter: 'helper' }), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });

  it('refuses a user the permission API does not know', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent({ commenter: 'unknown' }), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });
});

describe('which comments count', () => {
  it('ignores the command with extra words: no API call at all', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent({ body: '/rerun-failed please' }), api);
    assert.deepEqual(api.calls, []);
  });

  it('ignores a comment on an issue: no API call at all', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent({ onIssue: true }), api);
    assert.deepEqual(api.calls, []);
  });

  it('accepts a space or a new line after the command', async () => {
    // Only after it: the job's `if:` needs the comment to start with the command.
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent({ body: '/rerun-failed \r\n' }), api);
    assert.deepEqual(reruns(api.calls), [1]);
  });
});

describe('which CI run is used', () => {
  it('lists the CI runs of the head commit that the API gives for the PR', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })] });
    await runCommand(commentEvent(), api);
    const [, params] = api.calls.find(([name]) => name === 'listWorkflowRuns');
    assert.deepEqual(params, {
      owner: 'esphome',
      repo: 'esphome',
      workflow_id: 'ci.yml',
      event: 'pull_request',
      head_sha: HEAD_SHA,
      per_page: 1,
    });
  });

  it('uses only the newest run of the PR', async () => {
    // Run 1 failed, then the PR was closed and reopened and run 2 passed.
    const api = fakeGitHub({ runs: [ciRun({ id: 2, conclusion: 'success' }), ciRun({ id: 1 })] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });

  it('does not re-run a red run of an older commit', async () => {
    // The PR got a new commit, and CI has not started for it yet.
    const api = fakeGitHub({ runs: [ciRun({ id: 1, sha: OLD_SHA })] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });

  it('answers confused when the head commit has no CI run', async () => {
    const api = fakeGitHub({ runs: [] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });
});

describe('which runs are re-run', () => {
  it('does not re-run a run that is still in progress; the answer is eyes', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1, status: 'in_progress' })] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['eyes']);
  });

  // action_required: the run waits for a maintainer's approval.
  for (const conclusion of ['success', 'cancelled', 'action_required']) {
    it(`does not re-run a run with conclusion "${conclusion}"`, async () => {
      const api = fakeGitHub({ runs: [ciRun({ id: 1, conclusion })] });
      await runCommand(commentEvent(), api);
      assert.deepEqual(reruns(api.calls), []);
      assert.deepEqual(reactions(api.calls), ['confused']);
    });
  }

  it('re-runs again when the first re-run failed too', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1, attempt: 2 })] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), [1]);
    assert.deepEqual(reactions(api.calls), ['rocket']);
  });

  it('refuses a third re-run after two re-runs', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1, attempt: 3 })] });
    await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), []);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });
});

describe('errors', () => {
  // For example, the comment was deleted before the job ran.
  const reactionError = httpError(404, 'Not Found');

  it('answers confused and passes when GitHub refuses the re-run with a 403', async () => {
    // GitHub refuses, for example, to re-run a run older than 30 days.
    const rerunError = httpError(403, 'Unable to re-run this workflow run');
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })], rerunError });
    const log = await runCommand(commentEvent(), api);
    assert.deepEqual(reactions(api.calls), ['confused']);
    assert.ok(log.includes('GitHub refused to re-run the failed jobs of CI run 1: Unable to re-run this workflow run'));
  });

  it('answers confused and fails the step when the re-run fails otherwise', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })], rerunError: httpError(500, 'Server Error') });
    await assert.rejects(runCommand(commentEvent(), api), /Server Error/);
    assert.deepEqual(reactions(api.calls), ['confused']);
  });

  it('still passes when the reaction fails after the re-run started', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })], reactionError });
    const log = await runCommand(commentEvent(), api);
    assert.deepEqual(reruns(api.calls), [1]);
    assert.ok(log.includes('Could not add the rocket reaction: Not Found'));
  });

  it('fails with the re-run error when the re-run and the reaction both fail', async () => {
    const api = fakeGitHub({ runs: [ciRun({ id: 1 })], rerunError: httpError(500, 'Server Error'), reactionError });
    await assert.rejects(runCommand(commentEvent(), api), /Server Error/);
  });
});

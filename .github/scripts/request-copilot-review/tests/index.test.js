const { test } = require('node:test');
const assert = require('node:assert/strict');
const requestCopilotReview = require('../index');

const REPO = { owner: 'esphome', repo: 'esphome' };

const OLD = '2020-01-01T00:00:00Z';
const LATER = '2020-01-01T00:05:00Z';
// The account the workflow's token belongs to.
const REQUESTER = 'esphome-review-bot';
// "ours" is a request filed by REQUESTER; anything else stands for the ruleset or a maintainer.
const copilotRequest = (id, by, createdAt = LATER) => ({
  id,
  event: 'review_requested',
  actor: { login: by === 'ours' ? REQUESTER : 'someone', type: 'User' },
  requested_reviewer: { login: 'Copilot' },
  created_at: createdAt,
});
const copilotReview = (id, submittedAt, login = 'Copilot') => ({
  id,
  event: 'reviewed',
  user: { login },
  submitted_at: submittedAt,
});
const copilotRemoval = (id) => ({
  id,
  event: 'review_request_removed',
  requested_reviewer: { login: 'Copilot' },
  created_at: LATER,
});

// `before` and `after` are the timeline reads around the request, `timeline` sets both, and
// `timelineError` makes that read throw.
function makeGithub(opts = {}) {
  const calls = [];
  const reads = [
    { name: 'before', events: opts.timeline ?? opts.before ?? [] },
    { name: 'after', events: opts.timeline ?? opts.after ?? [copilotRequest(1, 'ours')] },
  ];
  const listEventsForTimeline = () => {};
  return {
    calls,
    github: {
      paginate: async (fn) => {
        assert.equal(fn, listEventsForTimeline);
        const read = reads.shift();
        assert.ok(read, 'the timeline is read at most twice');
        if (opts.timelineError === read.name) throw new Error('secondary rate limit');
        return read.events;
      },
      rest: {
        users: {
          getAuthenticated: async () => {
            if (opts.whoamiStatus) {
              throw Object.assign(new Error('Bad credentials'), { status: opts.whoamiStatus });
            }
            return { data: { login: REQUESTER } };
          },
        },
        issues: { listEventsForTimeline },
        pulls: {
          requestReviewers: async (args) => {
            if (opts.fail) {
              throw Object.assign(new Error(opts.fail.message ?? 'Reviews may only be requested from collaborators'), {
                status: opts.fail.status ?? 422,
              });
            }
            calls.push(args);
          },
        },
      },
    },
  };
}

function makeCore() {
  const warnings = [];
  const infos = [];
  const failures = [];
  return {
    warnings,
    infos,
    failures,
    core: {
      info: (m) => void infos.push(m),
      warning: (m) => void warnings.push(m),
      setFailed: (m) => void failures.push(m),
    },
  };
}

async function run(githubOpts, type = 'User') {
  const { github, calls } = makeGithub(githubOpts);
  const { core, infos, warnings, failures } = makeCore();
  const context = { repo: REPO, payload: { pull_request: { number: 9, user: { type } } } };
  await requestCopilotReview({ github, context, core });
  return { calls, infos, warnings, failures };
}

test('a PR event requests a Copilot review', async () => {
  const { calls, infos, warnings } = await run();
  assert.deepEqual(calls, [{ ...REPO, pull_number: 9, reviewers: ['copilot-pull-request-reviewer[bot]'] }]);
  assert.deepEqual(warnings, []);
  assert.match(infos[0], /Requested a Copilot review on #9/);
});

test('bot PRs are skipped', async () => {
  const { calls } = await run({}, 'Bot');
  assert.deepEqual(calls, []);
});

test('a failed request warns instead of failing', async () => {
  const { warnings, failures } = await run({ fail: true });
  assert.match(warnings[0], /Could not request a Copilot review on #9/);
  assert.deepEqual(failures, []);
});

test('a token without write access fails the run so it gets fixed', async () => {
  const { warnings, failures } = await run({
    fail: { status: 403, message: 'Resource not accessible by personal access token' },
  });
  assert.deepEqual(warnings, []);
  assert.match(failures[0], /COPILOT_REVIEW_TOKEN cannot request reviewers on esphome\/esphome/);
});

test('a rate limited request warns instead of failing', async () => {
  const { warnings, failures } = await run({
    fail: { status: 403, message: 'You have exceeded a secondary rate limit' },
  });
  assert.match(warnings[0], /secondary rate limit/);
  assert.deepEqual(failures, []);
});

test('an accepted request GitHub never recorded warns', async () => {
  const { warnings } = await run({ after: [{ id: 1, event: 'labeled' }] });
  assert.match(warnings[0], /recorded none/);
});

test('a request filed by another account does not confirm ours', async () => {
  const { warnings } = await run({ after: [copilotRequest(1, 'other'), copilotReview(2, LATER)] });
  assert.match(warnings[0], /recorded none/);
});

test('an earlier request by the same account does not confirm this one', async () => {
  const { warnings } = await run({ timeline: [copilotRequest(1, 'ours', OLD), copilotReview(2, LATER)] });
  assert.match(warnings[0], /recorded none/);
});

test('a Copilot review still pending is logged, not warned', async () => {
  const { infos, warnings } = await run({ timeline: [copilotReview(1, OLD), copilotRequest(2, 'other')] });
  assert.deepEqual(warnings, []);
  assert.match(infos[0], /requested by someone is still pending on #9/);
});

test('a Copilot request that was removed is not pending', async () => {
  const { warnings } = await run({ timeline: [copilotRequest(1, 'other'), copilotRemoval(2)] });
  assert.match(warnings[0], /recorded none/);
});

test('a completed review under the bot account login ends the pending request', async () => {
  const timeline = [
    copilotRequest(1, 'other', OLD),
    copilotReview(2, LATER, 'copilot-pull-request-reviewer[bot]'),
  ];
  const { warnings } = await run({ timeline });
  assert.match(warnings[0], /recorded none/);
});

test('a failed timeline read after the request warns instead of failing', async () => {
  const { calls, warnings } = await run({ timelineError: 'after' });
  assert.equal(calls.length, 1);
  assert.match(warnings[0], /could not confirm it: secondary rate limit/);
});

test('a failed baseline read still requests the review, unconfirmed', async () => {
  const { calls, warnings } = await run({ timelineError: 'before' });
  assert.equal(calls.length, 1);
  assert.equal(warnings.length, 1);
  assert.match(warnings[0], /goes unconfirmed: secondary rate limit/);
});

test('a rejected token fails the run so it gets rotated', async () => {
  const { calls, failures } = await run({ whoamiStatus: 401 });
  assert.equal(calls.length, 0);
  assert.match(failures[0], /COPILOT_REVIEW_TOKEN was rejected/);
});

test('a failed account lookup still requests the review, unconfirmed', async () => {
  const { calls, warnings, failures } = await run({ whoamiStatus: 502 });
  assert.equal(calls.length, 1);
  assert.deepEqual(failures, []);
  assert.equal(warnings.length, 1);
  assert.match(warnings[0], /Could not identify the requesting account/);
});

const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { handleReviews } = require('../reviews');
const { BOT_COMMENT_MARKER } = require('../constants');

const CONTEXT = {
  repo: { owner: 'esphome', repo: 'esphome' },
  issue: { number: 123 },
  payload: { pull_request: { user: { login: 'author' } } }
};

// GitHub API mock recording review calls; `reviews` is what listReviews returns.
function makeGithub(reviews) {
  const calls = { create: [], update: [], dismiss: [] };
  return {
    calls,
    rest: {
      pulls: {
        listReviews: async () => ({ data: reviews }),
        createReview: async (args) => { calls.create.push(args); },
        updateReview: async (args) => { calls.update.push(args); },
        dismissReview: async (args) => { calls.dismiss.push(args); }
      }
    }
  };
}

function botReview(state) {
  return {
    id: 42,
    state,
    user: { type: 'Bot', login: 'esphome[bot]' },
    body: `${BOT_COMMENT_MARKER}\nreview body`
  };
}

function run(github, labels) {
  return handleReviews(github, CONTEXT, labels, labels.length, [], [], 0, 0, 15, 1000);
}

describe('handleReviews', () => {
  it('creates a review when there is none and a reviewable label is set', async () => {
    const github = makeGithub([]);
    await run(github, ['too-big']);
    assert.equal(github.calls.create.length, 1);
    assert.equal(github.calls.create[0].event, 'REQUEST_CHANGES');
    assert.equal(github.calls.update.length, 0);
  });

  it('updates an active bot review instead of creating a new one', async () => {
    const github = makeGithub([botReview('CHANGES_REQUESTED')]);
    await run(github, ['too-big']);
    assert.equal(github.calls.create.length, 0);
    assert.equal(github.calls.update.length, 1);
    assert.equal(github.calls.update[0].review_id, 42);
  });

  it('updates a dismissed bot review instead of creating a new one', async () => {
    // Re-creating the review would emit a new changes-requested event and the
    // bot would convert the PR back to draft after the author marked it ready.
    const github = makeGithub([botReview('DISMISSED')]);
    await run(github, ['too-big']);
    assert.equal(github.calls.create.length, 0);
    assert.equal(github.calls.update.length, 1);
    assert.equal(github.calls.update[0].review_id, 42);
  });

  it('dismisses an active bot review when no reviewable labels remain', async () => {
    const github = makeGithub([botReview('CHANGES_REQUESTED')]);
    await run(github, ['small-pr']);
    assert.equal(github.calls.dismiss.length, 1);
    assert.equal(github.calls.dismiss[0].review_id, 42);
  });

  it('does not dismiss an already dismissed review', async () => {
    const github = makeGithub([botReview('DISMISSED')]);
    await run(github, ['small-pr']);
    assert.equal(github.calls.dismiss.length, 0);
  });

  it('updates the newest bot review when several exist', async () => {
    const old = { ...botReview('DISMISSED'), id: 1 };
    const active = { ...botReview('CHANGES_REQUESTED'), id: 2 };
    const github = makeGithub([old, active]);
    await run(github, ['too-big']);
    assert.equal(github.calls.create.length, 0);
    assert.equal(github.calls.update.length, 1);
    assert.equal(github.calls.update[0].review_id, 2);
  });

  it('ignores reviews from humans', async () => {
    const review = {
      id: 7,
      state: 'CHANGES_REQUESTED',
      user: { type: 'User', login: 'maintainer' },
      body: 'please fix'
    };
    const github = makeGithub([review]);
    await run(github, ['too-big']);
    assert.equal(github.calls.create.length, 1);
    assert.equal(github.calls.update.length, 0);
  });
});

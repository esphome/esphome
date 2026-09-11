const { describe, it, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const run = require('../index');

// Builds a fake `github` client covering only the calls index.js makes.
// `commentPages` feeds the dedup iterator; `listReviewComments`/`listReviews`
// calls are counted so short-circuit ordering (rewrite -> dedup -> human
// review) can be asserted without a live API.
function makeGithub({ compareCommits, commentPages = [], reviewComments = [], reviews = [] }) {
  const calls = { listReviewComments: 0, listReviews: 0, createComment: [] };
  const github = {
    rest: {
      repos: {
        compareCommits: async () => {
          if (compareCommits.throws) throw compareCommits.throws;
          return { data: compareCommits.data };
        },
      },
      issues: {
        createComment: async params => {
          calls.createComment.push(params);
        },
      },
      pulls: {
        listReviewComments: async () => {
          calls.listReviewComments += 1;
          return { data: reviewComments };
        },
        listReviews: async () => {
          calls.listReviews += 1;
          return { data: reviews };
        },
      },
    },
    paginate: Object.assign(
      async (fn, params) => {
        const { data } = await fn(params);
        return data;
      },
      {
        // eslint-disable-next-line require-yield
        iterator: async function* () {
          for (const page of commentPages) {
            yield { data: page };
          }
        },
      },
    ),
  };
  return { github, calls };
}

function makeContext({ branch = 'feature/x', prNumber = 42, authorLogin = 'pr-author' } = {}) {
  return {
    payload: { pull_request: { head: { ref: branch }, number: prNumber, user: { login: authorLogin } } },
    repo: { owner: 'esphome', repo: 'esphome' },
  };
}

function makeCore() {
  const messages = [];
  return { core: { info: msg => messages.push(msg) }, messages };
}

describe('warn-on-force-push index', () => {
  let originalEnv;
  beforeEach(() => {
    originalEnv = { BEFORE: process.env.BEFORE, AFTER: process.env.AFTER };
    process.env.BEFORE = 'before-sha';
    process.env.AFTER = 'after-sha';
  });
  afterEach(() => {
    process.env.BEFORE = originalEnv.BEFORE;
    process.env.AFTER = originalEnv.AFTER;
  });

  it('does nothing on a fast-forward, and never queries comments or reviews', async () => {
    const { github, calls } = makeGithub({ compareCommits: { data: { status: 'ahead', behind_by: 0 } } });
    const { core } = makeCore();
    await run({ github, context: makeContext(), core });
    assert.equal(calls.createComment.length, 0);
    assert.equal(calls.listReviewComments, 0);
    assert.equal(calls.listReviews, 0);
  });

  it('suppresses on a rewrite when the marker comment already exists, before checking reviews', async () => {
    const { github, calls } = makeGithub({
      compareCommits: { data: { status: 'diverged', behind_by: 1 } },
      commentPages: [[{ body: 'unrelated' }], [{ body: '<!-- force-push-warning -->already posted' }]],
    });
    const { core } = makeCore();
    await run({ github, context: makeContext(), core });
    assert.equal(calls.createComment.length, 0);
    assert.equal(calls.listReviewComments, 0);
    assert.equal(calls.listReviews, 0);
  });

  it('does not post when no marker exists but there is no human review activity', async () => {
    const { github, calls } = makeGithub({
      compareCommits: { data: { status: 'diverged', behind_by: 1 } },
      commentPages: [],
      reviewComments: [],
      reviews: [],
    });
    const { core } = makeCore();
    await run({ github, context: makeContext(), core });
    assert.equal(calls.createComment.length, 0);
  });

  it('posts exactly one comment, with the marker, on a rewrite with human review and no prior warning', async () => {
    const { github, calls } = makeGithub({
      compareCommits: { data: { status: 'diverged', behind_by: 1 } },
      commentPages: [],
      reviewComments: [
        { user: { type: 'User', login: 'bdraco' }, body: '```suggestion\nfix\n```' },
      ],
    });
    const { core } = makeCore();
    await run({ github, context: makeContext({ branch: 'feature/x' }), core });
    assert.equal(calls.createComment.length, 1);
    assert.match(calls.createComment[0].body, /<!-- force-push-warning -->/);
    assert.equal(calls.createComment[0].issue_number, 42);
  });

  it('does not post when the only "review activity" is the PR author self-commenting', async () => {
    const { github, calls } = makeGithub({
      compareCommits: { data: { status: 'diverged', behind_by: 1 } },
      commentPages: [],
      reviewComments: [
        { user: { type: 'User', login: 'pr-author' }, body: '```suggestion\nfix\n```' },
      ],
      reviews: [{ user: { type: 'User', login: 'pr-author' }, state: 'COMMENTED' }],
    });
    const { core } = makeCore();
    await run({ github, context: makeContext({ authorLogin: 'pr-author' }), core });
    assert.equal(calls.createComment.length, 0);
  });
});

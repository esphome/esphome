const {
  BOT_COMMENT_MARKER,
  CODEOWNERS_MARKER,
  TOO_BIG_MARKER,
  DEPRECATED_COMPONENT_MARKER,
  ORG_FORK_MARKER
} = require('./constants');

// Generate review messages
function generateReviewMessages(finalLabels, originalLabelCount, deprecatedInfo, prFiles, totalAdditions, totalDeletions, prAuthor, MAX_LABELS, TOO_BIG_THRESHOLD) {
  const messages = [];

  // Deprecated component message
  if (finalLabels.includes('deprecated-component') && deprecatedInfo && deprecatedInfo.length > 0) {
    let message = `${DEPRECATED_COMPONENT_MARKER}\n### ⚠️ Deprecated Component\n\n`;
    message += `Hey there @${prAuthor},\n`;
    message += `This PR modifies one or more deprecated components. Please be aware:\n\n`;

    for (const info of deprecatedInfo) {
      message += `#### Component: \`${info.component}\`\n`;
      message += `${info.message}\n\n`;
    }

    message += `Consider migrating to the recommended alternative if applicable.`;

    messages.push(message);
  }

  // Too big message
  if (finalLabels.includes('too-big')) {
    const testAdditions = prFiles
      .filter(file => file.filename.startsWith('tests/'))
      .reduce((sum, file) => sum + (file.additions || 0), 0);
    const testDeletions = prFiles
      .filter(file => file.filename.startsWith('tests/'))
      .reduce((sum, file) => sum + (file.deletions || 0), 0);
    const nonTestChanges = (totalAdditions - testAdditions) - (totalDeletions - testDeletions);

    const tooManyLabels = originalLabelCount > MAX_LABELS;
    const tooManyChanges = nonTestChanges > TOO_BIG_THRESHOLD;

    let message = `${TOO_BIG_MARKER}\n### 📦 Pull Request Size\n\n`;

    message +=
      `Hey @${prAuthor}, thanks for the contribution! Just a heads up, ` +
      `this PR is on the large side `;

    if (tooManyLabels && tooManyChanges) {
      message +=
        `(${nonTestChanges} line changes excluding tests, across ` +
        `${originalLabelCount} different components/areas)`;
    } else if (tooManyLabels) {
      message +=
        `(it touches ${originalLabelCount} different components/areas)`;
    } else {
      message += `(${nonTestChanges} line changes excluding tests)`;
    }

    message += `, which makes it harder for maintainers to review.\n\n`;
    message +=
      `Smaller, focused PRs tend to be reviewed much faster since they ` +
      `fit into the short gaps between other maintainer work; large ones ` +
      `often have to wait for a rare long uninterrupted block of time. ` +
      `If you can break this up into smaller pieces that can be reviewed ` +
      `independently, it will almost certainly land faster overall.\n\n`;
    message +=
      `Before putting more time in, it's also worth popping into ` +
      `\`#devs\` on [Discord](https://esphome.io/chat) so we can help ` +
      `you scope things and flag anything already in flight.\n\n`;
    message +=
      `For more details (including how to split the work up), see: ` +
      `https://developers.esphome.io/contributing/submitting-your-work/` +
      `#how-to-approach-large-submissions`;

    messages.push(message);
  }

  // CODEOWNERS message
  if (finalLabels.includes('needs-codeowners')) {
    const message = `${CODEOWNERS_MARKER}\n### 👥 Code Ownership\n\n` +
      `Hey there @${prAuthor},\n` +
      `Thanks for submitting this pull request! Can you add yourself as a codeowner for this integration? ` +
      `This way we can notify you if a bug report for this integration is reported.\n\n` +
      `In \`__init__.py\` of the integration, please add:\n\n` +
      `\`\`\`python\nCODEOWNERS = ["@${prAuthor}"]\n\`\`\`\n\n` +
      `And run \`script/build_codeowners.py\``;

    messages.push(message);
  }

  return messages;
}

// Handle reviews
async function handleReviews(github, context, finalLabels, originalLabelCount, deprecatedInfo, prFiles, totalAdditions, totalDeletions, MAX_LABELS, TOO_BIG_THRESHOLD) {
  const { owner, repo } = context.repo;
  const pr_number = context.issue.number;
  const prAuthor = context.payload.pull_request.user.login;

  const reviewMessages = generateReviewMessages(finalLabels, originalLabelCount, deprecatedInfo, prFiles, totalAdditions, totalDeletions, prAuthor, MAX_LABELS, TOO_BIG_THRESHOLD);
  const hasReviewableLabels = finalLabels.some(label =>
    ['too-big', 'needs-codeowners', 'deprecated-component'].includes(label)
  );

  const { data: reviews } = await github.rest.pulls.listReviews({
    owner,
    repo,
    pull_number: pr_number
  });

  const botReviews = reviews.filter(review =>
    review.user.type === 'Bot' &&
    review.state === 'CHANGES_REQUESTED' &&
    review.body && review.body.includes(BOT_COMMENT_MARKER)
  );

  if (hasReviewableLabels) {
    const reviewBody = `${BOT_COMMENT_MARKER}\n\n${reviewMessages.join('\n\n---\n\n')}`;

    if (botReviews.length > 0) {
      // Update existing review
      await github.rest.pulls.updateReview({
        owner,
        repo,
        pull_number: pr_number,
        review_id: botReviews[0].id,
        body: reviewBody
      });
      console.log('Updated existing bot review');
    } else {
      // Create new review
      await github.rest.pulls.createReview({
        owner,
        repo,
        pull_number: pr_number,
        body: reviewBody,
        event: 'REQUEST_CHANGES'
      });
      console.log('Created new bot review');
    }
  } else if (botReviews.length > 0) {
    // Dismiss existing reviews
    for (const review of botReviews) {
      try {
        await github.rest.pulls.dismissReview({
          owner,
          repo,
          pull_number: pr_number,
          review_id: review.id,
          message: 'Review dismissed: All requirements have been met'
        });
        console.log(`Dismissed bot review ${review.id}`);
      } catch (error) {
        console.log(`Failed to dismiss review ${review.id}:`, error.message);
      }
    }
  }
}

// Handle maintainer access warning comment
async function handleMaintainerAccessComment(github, context, maintainerAccess) {
  if (!maintainerAccess) {
    return;
  }

  const { owner, repo } = context.repo;
  const pr_number = context.issue.number;
  const prAuthor = context.payload.pull_request.user.login;

  // Check if we already posted the warning (iterate pages to exit early)
  let existingComment;
  for await (const { data: comments } of github.paginate.iterator(
    github.rest.issues.listComments,
    { owner, repo, issue_number: pr_number }
  )) {
    existingComment = comments.find(comment =>
      comment.user.type === 'Bot' &&
      comment.body && comment.body.includes(ORG_FORK_MARKER)
    );
    if (existingComment) {
      break;
    }
  }

  if (existingComment) {
    console.log('Maintainer access warning comment already exists, skipping');
    return;
  }

  let body;
  if (maintainerAccess.isOrgFork) {
    body = `${ORG_FORK_MARKER}\n### ⚠️ Organization Fork Detected\n\n` +
      `Hey there @${prAuthor},\n` +
      `It looks like this PR was submitted from a fork owned by the **${maintainerAccess.orgName}** organization. ` +
      `GitHub does not allow maintainers to push changes to pull request branches when the fork is owned by an organization. ` +
      `This means we won't be able to make small adjustments or fixups to your PR directly.\n\n` +
      `To allow maintainer collaboration, please re-submit this PR from a personal fork instead.\n\n` +
      `See: [Setting up the local repository](https://developers.esphome.io/contributing/development-environment/?h=org#set-up-the-local-repository) for more details.`;
  } else {
    body = `${ORG_FORK_MARKER}\n### ⚠️ Maintainer Access Disabled\n\n` +
      `Hey there @${prAuthor},\n` +
      `It looks like this PR does not have the "Allow edits from maintainers" option enabled. ` +
      `This means we won't be able to make small adjustments or fixups to your PR directly.\n\n` +
      `Please enable this option in the PR sidebar to allow maintainer collaboration.`;
  }

  await github.rest.issues.createComment({
    owner,
    repo,
    issue_number: pr_number,
    body
  });
  console.log('Created maintainer access warning comment');
}

module.exports = {
  handleReviews,
  handleMaintainerAccessComment
};

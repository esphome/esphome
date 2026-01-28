const { MANAGED_LABELS } = require('./constants');
const {
  detectMergeBranch,
  detectComponentPlatforms,
  detectNewComponents,
  detectNewPlatforms,
  detectCoreChanges,
  detectPRSize,
  detectDashboardChanges,
  detectGitHubActionsChanges,
  detectCodeOwner,
  detectTests,
  detectPRTemplateCheckboxes,
  detectRequirements
} = require('./detectors');
const { handleReviews } = require('./reviews');
const { applyLabels, removeOldLabels } = require('./labels');

// Fetch API data
async function fetchApiData() {
  try {
    const response = await fetch('https://data.esphome.io/components.json');
    const componentsData = await response.json();
    return {
      targetPlatforms: componentsData.target_platforms || [],
      platformComponents: componentsData.platform_components || []
    };
  } catch (error) {
    console.log('Failed to fetch components data from API:', error.message);
    return { targetPlatforms: [], platformComponents: [] };
  }
}

module.exports = async ({ github, context }) => {
  // Environment variables
  const SMALL_PR_THRESHOLD = parseInt(process.env.SMALL_PR_THRESHOLD);
  const MAX_LABELS = parseInt(process.env.MAX_LABELS);
  const TOO_BIG_THRESHOLD = parseInt(process.env.TOO_BIG_THRESHOLD);
  const COMPONENT_LABEL_THRESHOLD = parseInt(process.env.COMPONENT_LABEL_THRESHOLD);

  // Global state
  const { owner, repo } = context.repo;
  const pr_number = context.issue.number;

  // Get current labels and PR data
  const { data: currentLabelsData } = await github.rest.issues.listLabelsOnIssue({
    owner,
    repo,
    issue_number: pr_number
  });
  const currentLabels = currentLabelsData.map(label => label.name);
  const managedLabels = currentLabels.filter(label =>
    label.startsWith('component: ') || MANAGED_LABELS.includes(label)
  );

  // Check for mega-PR early - if present, skip most automatic labeling
  const isMegaPR = currentLabels.includes('mega-pr');

  // Get all PR files with automatic pagination
  const prFiles = await github.paginate(
    github.rest.pulls.listFiles,
    {
      owner,
      repo,
      pull_number: pr_number
    }
  );

  // Calculate data from PR files
  const changedFiles = prFiles.map(file => file.filename);
  const totalAdditions = prFiles.reduce((sum, file) => sum + (file.additions || 0), 0);
  const totalDeletions = prFiles.reduce((sum, file) => sum + (file.deletions || 0), 0);
  const totalChanges = totalAdditions + totalDeletions;

  console.log('Current labels:', currentLabels.join(', '));
  console.log('Changed files:', changedFiles.length);
  console.log('Total changes:', totalChanges);
  if (isMegaPR) {
    console.log('Mega-PR detected - applying limited labeling logic');
  }

  // Fetch API data
  const apiData = await fetchApiData();
  const baseRef = context.payload.pull_request.base.ref;

  // Early exit for release and beta branches only
  if (baseRef === 'release' || baseRef === 'beta') {
    const branchLabels = await detectMergeBranch(context);
    const finalLabels = Array.from(branchLabels);

    console.log('Computed labels (merge branch only):', finalLabels.join(', '));

    // Apply labels
    await applyLabels(github, context, finalLabels);

    // Remove old managed labels
    await removeOldLabels(github, context, managedLabels, finalLabels);

    return;
  }

  // Run all strategies
  const [
    branchLabels,
    componentLabels,
    newComponentLabels,
    newPlatformLabels,
    coreLabels,
    sizeLabels,
    dashboardLabels,
    actionsLabels,
    codeOwnerLabels,
    testLabels,
    checkboxLabels,
  ] = await Promise.all([
    detectMergeBranch(context),
    detectComponentPlatforms(changedFiles, apiData),
    detectNewComponents(prFiles),
    detectNewPlatforms(prFiles, apiData),
    detectCoreChanges(changedFiles),
    detectPRSize(prFiles, totalAdditions, totalDeletions, totalChanges, isMegaPR, SMALL_PR_THRESHOLD, TOO_BIG_THRESHOLD),
    detectDashboardChanges(changedFiles),
    detectGitHubActionsChanges(changedFiles),
    detectCodeOwner(github, context, changedFiles),
    detectTests(changedFiles),
    detectPRTemplateCheckboxes(context),
  ]);

  // Combine all labels
  const allLabels = new Set([
    ...branchLabels,
    ...componentLabels,
    ...newComponentLabels,
    ...newPlatformLabels,
    ...coreLabels,
    ...sizeLabels,
    ...dashboardLabels,
    ...actionsLabels,
    ...codeOwnerLabels,
    ...testLabels,
    ...checkboxLabels,
  ]);

  // Detect requirements based on all other labels
  const requirementLabels = await detectRequirements(allLabels, prFiles, context);
  for (const label of requirementLabels) {
    allLabels.add(label);
  }

  let finalLabels = Array.from(allLabels);

  // For mega-PRs, exclude component labels if there are too many
  if (isMegaPR) {
    const componentLabels = finalLabels.filter(label => label.startsWith('component: '));
    if (componentLabels.length > COMPONENT_LABEL_THRESHOLD) {
      finalLabels = finalLabels.filter(label => !label.startsWith('component: '));
      console.log(`Mega-PR detected - excluding ${componentLabels.length} component labels (threshold: ${COMPONENT_LABEL_THRESHOLD})`);
    }
  }

  // Handle too many labels (only for non-mega PRs)
  const tooManyLabels = finalLabels.length > MAX_LABELS;
  const originalLabelCount = finalLabels.length;

  if (tooManyLabels && !isMegaPR && !finalLabels.includes('too-big')) {
    finalLabels = ['too-big'];
  }

  console.log('Computed labels:', finalLabels.join(', '));

  // Handle reviews
  await handleReviews(github, context, finalLabels, originalLabelCount, prFiles, totalAdditions, totalDeletions, MAX_LABELS, TOO_BIG_THRESHOLD);

  // Apply labels
  await applyLabels(github, context, finalLabels);

  // Remove old managed labels
  await removeOldLabels(github, context, managedLabels, finalLabels);
};

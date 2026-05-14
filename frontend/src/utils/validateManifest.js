import { PERTURBATION_TYPES } from '../constants/perturbations.js';
import { EXPECTATION_TYPES } from '../constants/expectations.js';

export function validateManifest({ testName, targetId, duration, perturbations, expectations }) {
  if (!testName.trim()) return 'Test name is required.';
  if (!targetId) return 'Select a target container.';
  if (!duration || duration <= 0) return 'Duration must be positive.';
  if (!perturbations.length) return 'Add at least one perturbation.';
  if (!expectations.length) return 'Add at least one expectation.';
  for (const p of perturbations) {
    const def = PERTURBATION_TYPES.find((t) => t.type === p.type);
    for (const param of def?.params || []) {
      if (param.required && !p.parameters?.[param.key]) return `Perturbation "${p.type}" missing "${param.label}".`;
    }
  }
  for (const e of expectations) {
    const def = EXPECTATION_TYPES.find((t) => t.type === e.type);
    for (const param of def?.params || []) {
      if (param.required && !e.parameters?.[param.key]) return `Expectation "${e.type}" missing "${param.label}".`;
    }
  }
  return null;
}

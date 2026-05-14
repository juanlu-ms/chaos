import { PERTURBATION_PARAMS } from '../constants/perturbations.js';
import { EXPECTATION_PARAMS } from '../constants/expectations.js';

function checkParams(item, params, kind) {
  const errors = [];
  for (const param of params || []) {
    if (param.required && !item.parameters?.[param.key]) {
      errors.push(`${kind} "${item.type}" missing "${param.label}".`);
    }
  }
  return errors;
}

/**
 * Returns all error strings. Empty array means valid.
 */
export function validateManifestAll({ testName, targetId, duration, perturbations, expectations }) {
  const errors = [];
  if (!testName?.trim()) errors.push('Test name is required.');
  if (!targetId) errors.push('Select a target container.');
  if (!duration || duration <= 0) errors.push('Duration must be positive.');
  if (!perturbations?.length) errors.push('Add at least one perturbation.');
  if (!expectations?.length) errors.push('Add at least one expectation.');
  for (const p of perturbations || []) {
    errors.push(...checkParams(p, PERTURBATION_PARAMS[p.type], 'Perturbation'));
  }
  for (const e of expectations || []) {
    errors.push(...checkParams(e, EXPECTATION_PARAMS[e.type], 'Expectation'));
  }
  return errors;
}

/**
 * Returns the first error string, or null. Backwards-compatible.
 */
export function validateManifest(input) {
  const errs = validateManifestAll(input);
  return errs.length ? errs[0] : null;
}

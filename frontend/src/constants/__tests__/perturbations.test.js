import { describe, it, expect } from 'vitest';
import { PERTURBATION_TYPES, PERTURBATION_PARAMS, defaultParamsFor } from '../perturbations.js';

describe('perturbations constants', () => {
  it('PERTURBATION_TYPES is derived from PERTURBATION_PARAMS keys', () => {
    const paramKeys = Object.keys(PERTURBATION_PARAMS);
    const typeKeys = PERTURBATION_TYPES.map((t) => t.type);
    expect(typeKeys).toEqual(paramKeys);
  });

  it('each PERTURBATION_TYPES entry has type and label', () => {
    for (const t of PERTURBATION_TYPES) {
      expect(t.type).toBeTruthy();
      expect(t.label).toBeTruthy();
    }
  });

  it('defaultParamsFor returns defaults for known types', () => {
    const params = defaultParamsFor('cpu_cap');
    expect(params).toHaveProperty('cpu_cores', '1');
  });

  it('defaultParamsFor returns empty for unknown types', () => {
    const params = defaultParamsFor('nonexistent');
    expect(Object.keys(params)).toHaveLength(0);
  });

  it('kill type has no required parameters', () => {
    expect(PERTURBATION_PARAMS.kill).toEqual([]);
  });
});

import { describe, it, expect } from 'vitest';
import { EXPECTATION_TYPES, EXPECTATION_PARAMS, defaultExpectationParams } from '../expectations.js';

describe('expectations constants', () => {
  it('EXPECTATION_TYPES matches EXPECTATION_PARAMS keys', () => {
    const paramKeys = Object.keys(EXPECTATION_PARAMS);
    const typeKeys = EXPECTATION_TYPES.map((t) => t.type);
    expect(typeKeys).toEqual(paramKeys);
  });

  it('each EXPECTATION_TYPES entry has type and label', () => {
    for (const t of EXPECTATION_TYPES) {
      expect(t.type).toBeTruthy();
      expect(t.label).toBeTruthy();
    }
  });

  it('defaultExpectationParams returns empty for parameterless types', () => {
    expect(Object.keys(defaultExpectationParams('container_running'))).toHaveLength(0);
  });

  it('defaultExpectationParams returns defaults for parameterized types', () => {
    const params = defaultExpectationParams('log_contains');
    expect(params).toHaveProperty('substring', '');
  });
});

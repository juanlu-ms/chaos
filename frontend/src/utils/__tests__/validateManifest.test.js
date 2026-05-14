import { describe, it, expect } from 'vitest';
import { validateManifest } from '../validateManifest.js';

describe('validateManifest', () => {
  const valid = () => ({
    testName: 'my-test',
    targetId: 'abc123',
    duration: 15,
    perturbations: [{ type: 'cpu_cap', parameters: { cpu_cores: '1' } }],
    expectations: [{ type: 'container_running', parameters: {} }],
  });

  it('returns null for valid manifest', () => {
    expect(validateManifest(valid())).toBeNull();
  });

  it('returns error for empty test name', () => {
    const m = valid();
    m.testName = '   ';
    expect(validateManifest(m)).toMatch(/test name/i);
  });

  it('returns error for missing target', () => {
    const m = valid();
    m.targetId = '';
    expect(validateManifest(m)).toMatch(/target/i);
  });

  it('returns error for zero duration', () => {
    const m = valid();
    m.duration = 0;
    expect(validateManifest(m)).toMatch(/duration/i);
  });

  it('returns error for no perturbations', () => {
    const m = valid();
    m.perturbations = [];
    expect(validateManifest(m)).toMatch(/perturbation/i);
  });

  it('returns error for no expectations', () => {
    const m = valid();
    m.expectations = [];
    expect(validateManifest(m)).toMatch(/expectation/i);
  });
});

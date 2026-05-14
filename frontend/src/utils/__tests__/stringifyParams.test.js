import { describe, it, expect } from 'vitest';
import { stringifyParams } from '../stringifyParams.js';

describe('stringifyParams', () => {
  it('converts values to strings', () => {
    expect(stringifyParams({ cpu_cores: 1 })).toEqual({ cpu_cores: '1' });
  });

  it('skips empty string values', () => {
    expect(stringifyParams({ a: 'hello', b: '' })).toEqual({ a: 'hello' });
  });

  it('skips null and undefined values', () => {
    expect(stringifyParams({ a: 'keep', b: null, c: undefined })).toEqual({ a: 'keep' });
  });

  it('handles empty object', () => {
    expect(stringifyParams({})).toEqual({});
  });

  it('handles null input', () => {
    expect(stringifyParams(null)).toEqual({});
  });
});

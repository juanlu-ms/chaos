import { describe, it, expect } from 'vitest';
import { shortId } from '../shortId.js';

describe('shortId', () => {
  it('returns em-dash for empty', () => {
    expect(shortId('')).toBe('—');
    expect(shortId(null)).toBe('—');
    expect(shortId(undefined)).toBe('—');
  });
  it('returns id when shorter than limit', () => {
    expect(shortId('abc')).toBe('abc');
  });
  it('truncates long ids', () => {
    expect(shortId('0123456789abcdef0000')).toBe('0123456789ab');
  });
  it('respects custom length', () => {
    expect(shortId('abcdefghij', 4)).toBe('abcd');
  });
});

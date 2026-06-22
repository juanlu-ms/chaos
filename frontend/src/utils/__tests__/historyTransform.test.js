import { describe, it, expect } from 'vitest';
import { historyRecordToResult } from '../historyTransform.js';

describe('historyRecordToResult', () => {
  const sampleRecord = {
    summary: {
      id: 'run-1000',
      started_at_unix: 1000000,
      ended_at_unix: 1005000,
      status: 'completed',
      normal_end_t: 10.0,
      chaos_end_t: 25.0,
      run_result: {
        passed: true,
        manifest_name: 'test-manifest',
        target_id: 'abc123',
        duration_s: 30.0,
        started_at: '2025-01-01T00:00:00Z',
        results: [
          { type: 'container_running', passed: true, message: 'ok' },
        ],
      },
    },
    manifest: {
      test_name: 'test-manifest',
      target: { id: 'abc123' },
      perturbations: [],
      expectations: [],
    },
    samples: [
      { t: 0.0, cpu_usage_percent: 50.0, memory_usage_mb: 256.0, network_rx_bps: 1000.0, network_tx_bps: 500.0, network_latency_ms: 2.5, phase: 'normal' },
      { t: 1.0, cpu_usage_percent: 80.0, memory_usage_mb: 512.0, phase: 'chaos' },
    ],
    logs: ['log line 1', 'log line 2'],
  };

  it('maps samples to chart data points with camelCase', () => {
    const { result } = historyRecordToResult(sampleRecord);
    expect(result.data).toHaveLength(2);
    expect(result.data[0]).toEqual({
      t: 0.0, cpu: 50.0, mem: 256.0, net: 1500.0, lat: 2.5, phase: 'normal',
    });
    expect(result.data[1]).toEqual({
      t: 1.0, cpu: 80.0, mem: 512.0, net: 0, lat: null, phase: 'chaos',
    });
  });

  it('maps zones from snake_case to camelCase', () => {
    const { result } = historyRecordToResult(sampleRecord);
    expect(result.zones).toEqual({ normalEnd: 10.0, chaosEnd: 25.0 });
  });

  it('extracts passed and results from embedded runResult (snake_case)', () => {
    const { result } = historyRecordToResult(sampleRecord);
    expect(result.passed).toBe(true);
    expect(result.results).toHaveLength(1);
    expect(result.results[0]).toEqual({ type: 'container_running', passed: true, message: 'ok' });
  });

  it('handles empty samples', () => {
    const empty = { ...sampleRecord, samples: [], logs: [] };
    const { result } = historyRecordToResult(empty);
    expect(result.data).toEqual([]);
    expect(result.logs).toEqual([]);
  });

  it('passes through manifest as-is', () => {
    const { manifest } = historyRecordToResult(sampleRecord);
    expect(manifest.test_name).toBe('test-manifest');
    expect(manifest.target.id).toBe('abc123');
  });

  it('handles runResult as camelCase (summary.runResult)', () => {
    const rec = {
      ...sampleRecord,
      summary: {
        ...sampleRecord.summary,
        runResult: { passed: false, results: [], manifest_name: 'x', target_id: 'y', duration_s: 1, started_at: '' },
        run_result: undefined,
      },
    };
    const { result } = historyRecordToResult(rec);
    expect(result.passed).toBe(false);
  });
});

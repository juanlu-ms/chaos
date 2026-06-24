export function historyRecordToResult(recordJson) {
  const summary = recordJson.summary || {};
  const runResult = summary.run_result || summary.runResult || summary;
  const rawManifest = recordJson.manifest || {};

  if (rawManifest.target_id && !rawManifest.target) {
    rawManifest.target = { id: rawManifest.target_id };
  }

  const manifest = rawManifest;

  const samples = recordJson.samples || [];
  const data = samples.map((s) => ({
    t: s.t ?? 0,
    cpu: s.cpu_usage_percent ?? 0,
    mem: s.memory_usage_mb ?? 0,
    net: (s.network_rx_bps || 0) + (s.network_tx_bps || 0),
    lat: s.network_latency_ms ?? null,
    phase: s.phase || 'normal',
  }));

  const zones = {
    normalEnd: summary.normal_end_t ?? 0,
    chaosEnd: summary.chaos_end_t ?? 0,
  };

  return {
    manifest,
    result: {
      passed: runResult.passed ?? false,
      results: runResult.results || [],
      error: summary.error || '',
      data,
      logs: recordJson.logs || [],
      zones,
    },
  };
}

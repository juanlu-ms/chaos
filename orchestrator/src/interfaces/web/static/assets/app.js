(() => {
  const PERTURBATION_TYPES = [
    { type: 'kill', label: 'Kill', params: [] },
    { type: 'memory_cap', label: 'Memory Cap', params: [{ key: 'limit_bytes', label: 'Limit (bytes)', placeholder: '268435456' }] },
    { type: 'cpu_cap', label: 'CPU Cap', params: [{ key: 'cpu_cores', label: 'CPU Cores', placeholder: '1' }] },
    { type: 'network_delay', label: 'Network Delay', params: [{ key: 'delay_ms', label: 'Delay (ms)', placeholder: '1000' }] },
    { type: 'network_cutoff', label: 'Network Cutoff', params: [
      { key: 'dst_ip', label: 'Dest IP', placeholder: 'target IP' },
      { key: 'dst_port', label: 'Dest Port', placeholder: 'target port' },
      { key: 'src_port', label: 'Source Port', placeholder: 'source port' },
    ]},
    { type: 'garbage_packet', label: 'Garbage Packet', params: [
      { key: 'corrupt_pct', label: 'Corrupt %', placeholder: 'e.g. 25' },
      { key: 'loss_pct', label: 'Loss %', placeholder: 'e.g. 10' },
      { key: 'duplicate_pct', label: 'Duplicate %', placeholder: 'e.g. 5' },
      { key: 'iface', label: 'Interface', placeholder: 'eth0' },
    ]},
  ];

  const EXPECTATION_PARAMS = {
    container_running: [],
    container_not_running: [],
    log_contains: [{ key: 'substring', placeholder: 'text to find in logs' }],
    log_not_contains: [{ key: 'substring', placeholder: 'text that must not appear' }],
    http_status: [
      { key: 'port', placeholder: '8000' },
      { key: 'path', placeholder: '/ping' },
      { key: 'expected_status', placeholder: '200' },
    ],
    http_latency: [
      { key: 'port', placeholder: '8000' },
      { key: 'path', placeholder: '/ping' },
      { key: 'max_latency_ms', placeholder: '1000' },
    ],
  };

  let currentStep = 1;
  let manifest = {};
  let chartData = { cpu: [], mem: [], net: [] };
  let charts = { cpuLive: null, memLive: null, netLive: null, cpuResults: null, memResults: null, netResults: null };
  let eventSource = null;
  let runActive = false;
  let targets = [];
  let limits = {};
  let perturbations = [];
  let expectations = [];
  let lastManifest = null;
  let timerInterval = null;
  let chaosStartIdx = -1;
  let recoveryStartIdx = -1;
  let accumulatedLogs = [];

  function showStep(n) {
    currentStep = n;
    document.querySelectorAll('.step').forEach(el => el.classList.remove('active'));
    document.getElementById('step-' + n).classList.add('active');
    document.querySelectorAll('.step-tab').forEach(el => {
      el.classList.toggle('active', parseInt(el.dataset.step) === n);
    });
  }

  document.querySelectorAll('.step-tab').forEach(btn => {
    btn.addEventListener('click', () => {
      showStep(parseInt(btn.dataset.step));
    });
  });

  async function loadTargets() {
    try {
      const res = await fetch('/api/targets');
      if (!res.ok) throw new Error('Failed to load targets');
      targets = await res.json();
      populateTargetSelect();
      document.getElementById('status-badge').textContent = 'Online';
      document.getElementById('status-badge').style.borderColor = 'var(--success)';
    } catch (err) {
      document.getElementById('status-badge').textContent = 'Offline';
      document.getElementById('status-badge').style.borderColor = 'var(--error)';
    }
  }

  function populateTargetSelect() {
    const sel = document.getElementById('field-target');
    sel.innerHTML = '<option value="" disabled selected>Select a target...</option>' +
      targets.map(t =>
        `<option value="${escapeHtml(t.id)}">${escapeHtml(t.name || t.id)}</option>`
      ).join('');
  }

  async function loadLimits() {
    try {
      const res = await fetch('/api/limits');
      if (!res.ok) throw new Error('Failed to load limits');
      limits = await res.json();
    } catch (err) {
      // keep defaults
    }
  }

  function renderPerturbations() {
    const container = document.getElementById('perturbation-list');
    if (!perturbations.length) {
      container.innerHTML = '<div class="empty-state">No perturbations added.</div>';
      return;
    }
    container.innerHTML = perturbations.map((p, i) => {
      const info = PERTURBATION_TYPES.find(t => t.type === p.type) || PERTURBATION_TYPES[0];
      const paramHtml = info.params.map(pr => {
        const val = p.params[pr.key] || '';
        return `<label class="pert-param-group"><span class="pert-param-label">${escapeHtml(pr.label)}</span><input class="pert-param" data-idx="${i}" data-key="${pr.key}" placeholder="${escapeHtml(pr.placeholder)}" value="${escapeHtml(val)}"></label>`;
      }).join('');
      return `<div class="perturbation-row">
        <select class="pert-type" data-idx="${i}">
          ${PERTURBATION_TYPES.map(t =>
            `<option value="${t.type}" ${t.type === p.type ? 'selected' : ''}>${t.label}</option>`
          ).join('')}
        </select>
        ${paramHtml}
        <button class="btn xs ghost remove-pert" data-idx="${i}" type="button">✕</button>
      </div>`;
    }).join('');
  }

  function addPerturbation() {
    perturbations.push({ type: 'kill', params: {} });
    renderPerturbations();
  }

  function removePerturbation(idx) {
    perturbations.splice(idx, 1);
    renderPerturbations();
  }

  function updatePerturbationType(idx, type) {
    const info = PERTURBATION_TYPES.find(t => t.type === type) || PERTURBATION_TYPES[0];
    const params = {};
    info.params.forEach(p => { params[p.key] = ''; });
    perturbations[idx].type = type;
    perturbations[idx].params = params;
    renderPerturbations();
  }

  function updatePerturbationParam(idx, key, value) {
    perturbations[idx].params[key] = value;
  }

  function renderExpectations() {
    const container = document.getElementById('expectation-list');
    if (!expectations.length) {
      container.innerHTML = '<div class="empty-state">No expectations added.</div>';
      return;
    }
    container.innerHTML = expectations.map((e, i) => {
      const paramDefs = EXPECTATION_PARAMS[e.type] || [];
      const paramHtml = paramDefs.map(pd => {
        const val = (e.parameters && e.parameters[pd.key]) || '';
        return `<input class="expect-param" data-key="${pd.key}" placeholder="${escapeHtml(pd.placeholder)}" value="${escapeHtml(val)}">`;
      }).join(' ');
      return `<div class="expectation-row">
        <select class="expect-type" data-idx="${i}">
          <option value="container_running" ${e.type === 'container_running' ? 'selected' : ''}>Container Running</option>
          <option value="container_not_running" ${e.type === 'container_not_running' ? 'selected' : ''}>Container Not Running</option>
          <option value="log_contains" ${e.type === 'log_contains' ? 'selected' : ''}>Log Contains</option>
          <option value="log_not_contains" ${e.type === 'log_not_contains' ? 'selected' : ''}>Log Not Contains</option>
          <option value="http_status" ${e.type === 'http_status' ? 'selected' : ''}>HTTP Status</option>
          <option value="http_latency" ${e.type === 'http_latency' ? 'selected' : ''}>HTTP Latency</option>
        </select>
        ${paramHtml}
        <button class="btn xs ghost remove-expect" type="button">✕</button>
      </div>`;
    }).join('');
  }

  function addExpectation() {
    expectations.push({ type: 'container_running', parameters: {} });
    renderExpectations();
  }

  function removeExpectation(idx) {
    expectations.splice(idx, 1);
    renderExpectations();
  }

  function readExpectations() {
    const rows = document.querySelectorAll('#expectation-list .expectation-row');
    expectations = [];
    rows.forEach(row => {
      const typeEl = row.querySelector('.expect-type');
      if (typeEl) {
        const type = typeEl.value;
        const params = {};
        row.querySelectorAll('.expect-param').forEach(inp => {
          if (inp.value !== '') params[inp.dataset.key] = inp.value;
        });
        expectations.push({ type, parameters: params });
      }
    });
  }

  function buildManifest() {
    readExpectations();
    const perturbationSpecs = perturbations.map(p => {
      const params = {};
      Object.keys(p.params).forEach(k => {
        if (p.params[k] !== '') params[k] = p.params[k];
      });
      return { type: p.type, parameters: params };
    });
    return {
      test_name: document.getElementById('field-name').value || 'Untitled Test',
      target: { id: document.getElementById('field-target').value },
      duration_s: parseInt(document.getElementById('field-duration').value, 10) || 10,
      perturbations: perturbationSpecs,
      expectations: expectations.map(e => ({ type: e.type, parameters: e.parameters || {} })),
    };
  }

  function initCharts() {
    if (charts.cpuLive) charts.cpuLive.destroy();
    if (charts.memLive) charts.memLive.destroy();
    if (charts.netLive) charts.netLive.destroy();
    charts.cpuLive = new BarChart('chart-cpu', { maxPoints: 60, unit: '%' });
    charts.memLive = new BarChart('chart-mem', { maxPoints: 60, unit: 'MB' });
    charts.netLive = new BarChart('chart-net', { maxPoints: 60, unit: 'bytes' });
    charts.cpuLive.resize();
    charts.memLive.resize();
    charts.netLive.resize();
  }

  function initTimelineCharts() {
    if (charts.cpuResults) charts.cpuResults.destroy();
    if (charts.memResults) charts.memResults.destroy();
    if (charts.netResults) charts.netResults.destroy();
    const count = Math.max(chartData.cpu.length, 60);
    charts.cpuResults = new TimelineChart('timeline-cpu', { maxPoints: count, unit: '%' });
    charts.memResults = new TimelineChart('timeline-mem', { maxPoints: count, unit: 'MB' });
    charts.netResults = new TimelineChart('timeline-net', { maxPoints: count, unit: 'bytes' });
  }

  function startTimer() {
    const startTime = Date.now();
    const el = document.getElementById('mon-timer');
    if (timerInterval) clearInterval(timerInterval);
    timerInterval = setInterval(() => {
      const s = Math.floor((Date.now() - startTime) / 1000);
      const m = String(Math.floor(s / 60)).padStart(2, '0');
      const sec = String(s % 60).padStart(2, '0');
      el.textContent = m + ':' + sec;
    }, 200);
  }

  function stopTimer() {
    if (timerInterval) {
      clearInterval(timerInterval);
      timerInterval = null;
    }
  }

  function resetMonitorState() {
    chartData = { cpu: [], mem: [], net: [] };
    chaosStartIdx = -1;
    recoveryStartIdx = -1;
    accumulatedLogs = [];
    document.getElementById('mon-cpu-val').textContent = '--';
    document.getElementById('mon-mem-val').textContent = '--';
    document.getElementById('mon-net-val').textContent = '--';
    document.getElementById('mon-info-status').textContent = '--';
    document.getElementById('mon-info-cpu').textContent = '--';
    document.getElementById('mon-info-mem').textContent = '--';
    document.getElementById('mon-info-net').textContent = '--';
    document.getElementById('mon-logs').textContent = 'Waiting for state updates...';
    document.getElementById('mon-timer').textContent = '00:00';
  }

  async function runScenario() {
    if (runActive) return;
    const m = buildManifest();
    if (!m.target.id) {
      alert('Select a target container first.');
      return;
    }
    lastManifest = m;
    const runBtn = document.getElementById('run-btn');
    runBtn.disabled = true;
    runActive = true;
    if (eventSource) { eventSource.close(); eventSource = null; }
    resetMonitorState();
    initCharts();
    document.getElementById('mon-test-name').textContent = 'Running: ' + m.test_name;
    showStep(2);
    startTimer();
    try {
      const res = await fetch('/api/run', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(m),
      });
      if (!res.ok) {
        const err = await res.json().catch(() => ({}));
        throw new Error(err.error || 'HTTP ' + res.status);
      }
      eventSource = new EventSource('/events');
      eventSource.addEventListener('state', e => {
        try {
          const data = JSON.parse(e.data);
          document.getElementById('mon-info-status').textContent = data.status || '--';
          document.getElementById('mon-info-cpu').textContent =
            data.cpu_usage_percent != null ? data.cpu_usage_percent.toFixed(1) + '%' : '--';
          document.getElementById('mon-info-mem').textContent =
            data.memory_usage_mb != null ? data.memory_usage_mb.toFixed(1) + ' MB' : '--';
          document.getElementById('mon-info-net').textContent =
            data.network_rx_bytes != null && data.network_tx_bytes != null
              ? 'RX: ' + data.network_rx_bytes.toFixed(0) + ' B | TX: ' + data.network_tx_bytes.toFixed(0) + ' B'
              : '--';
          document.getElementById('mon-cpu-val').textContent =
            data.cpu_usage_percent != null ? data.cpu_usage_percent.toFixed(1) + '%' : '--';
          document.getElementById('mon-mem-val').textContent =
            data.memory_usage_mb != null ? data.memory_usage_mb.toFixed(1) + ' MB' : '--';
          document.getElementById('mon-net-val').textContent =
            data.network_rx_bytes != null && data.network_tx_bytes != null
              ? 'RX: ' + data.network_rx_bytes.toFixed(0) + ' B | TX: ' + data.network_tx_bytes.toFixed(0) + ' B'
              : '--';
          if (data.cpu_usage_percent != null) {
            chartData.cpu.push(data.cpu_usage_percent);
            charts.cpuLive.push(data.cpu_usage_percent);
          }
          if (data.memory_usage_mb != null) {
            chartData.mem.push(data.memory_usage_mb);
            charts.memLive.push(data.memory_usage_mb);
          }
          if (data.network_rx_bytes != null && data.network_tx_bytes != null) {
            const netTotal = data.network_rx_bytes + data.network_tx_bytes;
            chartData.net.push(netTotal);
            charts.netLive.push(netTotal);
          }
          if (data.phase) {
            if (data.phase === 'chaos' && chaosStartIdx < 0) {
              chaosStartIdx = chartData.cpu.length - 1;
            }
            if (data.phase === 'recovery' && recoveryStartIdx < 0 && chaosStartIdx >= 0) {
              recoveryStartIdx = chartData.cpu.length - 1;
            }
          }
          if (data.recent_logs && data.recent_logs.length) {
            data.recent_logs.forEach(line => {
              accumulatedLogs.push(line);
            });
            const logEl = document.getElementById('mon-logs');
            logEl.textContent = accumulatedLogs.join('\n');
            logEl.scrollTop = logEl.scrollHeight;
          }
        } catch (err) { /* ignore malformed state */ }
      });
      eventSource.addEventListener('complete', e => {
        try {
          const data = JSON.parse(e.data);
          showResults(data);
          showStep(3);
        } catch (err) { /* ignore */ }
        cleanupRun();
      });
      eventSource.addEventListener('error', e => {
        let errorMsg = 'Unknown error';
        try { const d = JSON.parse(e.data); errorMsg = d.error || errorMsg; } catch (_) {}
        showResults({ passed: false, results: [{ type: 'error', passed: false, message: errorMsg }], logs: accumulatedLogs });
        showStep(3);
        cleanupRun();
      });
    } catch (err) {
      showResults({ passed: false, results: [{ type: 'error', passed: false, message: err.message }], logs: accumulatedLogs });
      showStep(3);
      cleanupRun();
    }
  }

  function cleanupRun() {
    runActive = false;
    document.getElementById('run-btn').disabled = false;
    stopTimer();
    if (eventSource) {
      eventSource.close();
      eventSource = null;
    }
  }

  async function abortRun() {
    try {
      await fetch('/api/run/abort', { method: 'POST' });
    } catch (_) { /* ignore */ }
    cleanupRun();
    showStep(1);
  }

  function showResults(data) {
    const passed = data.passed === true;
    document.getElementById('r-duration').textContent = lastManifest ? lastManifest.duration_s + 's' : '--';
    document.getElementById('r-target').textContent = lastManifest ? escapeHtml(lastManifest.target.id) : '--';
    document.getElementById('r-perturbations').textContent = lastManifest ? lastManifest.perturbations.length : '--';
    const resultEl = document.getElementById('r-result');
    resultEl.textContent = passed ? 'PASS' : 'FAIL';
    resultEl.style.color = passed ? 'var(--success)' : 'var(--error)';
    const resArr = data.results || [];
    const logs = data.logs || accumulatedLogs;
    const cpuHistory = data.cpu_history || chartData.cpu;
    const memHistory = data.mem_history || chartData.mem;
    const netHistory = data.net_history || chartData.net;
    chartData.cpu = cpuHistory;
    chartData.mem = memHistory;
    chartData.net = netHistory;
    initTimelineCharts();
    cpuHistory.forEach(v => { if (v != null) charts.cpuResults.push(v); });
    memHistory.forEach(v => { if (v != null) charts.memResults.push(v); });
    netHistory.forEach(v => { if (v != null) charts.netResults.push(v); });
    setZones(chaosStartIdx, recoveryStartIdx);
    const exContainer = document.getElementById('r-expectations');
    if (resArr.length) {
      exContainer.innerHTML = resArr.map(r =>
        `<div class="result-item ${r.passed ? 'pass' : 'fail'}">
          <span class="result-icon">${r.passed ? '✓' : '✗'}</span>
          <span class="result-type">${escapeHtml(r.type || '')}</span>
          <span class="result-msg">${escapeHtml(r.message || '')}</span>
        </div>`
      ).join('');
    } else {
      exContainer.innerHTML = '<div class="empty-state">No expectations configured.</div>';
    }
    const logEl = document.getElementById('r-logs');
    if (Array.isArray(logs)) {
      logEl.textContent = logs.join('\n') || 'No logs recorded.';
    } else {
      logEl.textContent = logs || 'No logs recorded.';
    }
  }

  function setZones(chaosIdx, recoveryIdx) {
    [charts.cpuResults, charts.memResults, charts.netResults].forEach(c => {
      if (c) c.setZones(chaosIdx, recoveryIdx);
    });
  }

  function initTheme() {
    const saved = localStorage.getItem('chaos-theme') || 'amber';
    document.documentElement.setAttribute('data-theme', saved);
    document.getElementById('theme-select').value = saved;
    document.getElementById('theme-select').addEventListener('change', function () {
      document.documentElement.setAttribute('data-theme', this.value);
      localStorage.setItem('chaos-theme', this.value);
    });
  }

  function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  document.getElementById('add-perturbation').addEventListener('click', addPerturbation);
  document.getElementById('add-expectation').addEventListener('click', addExpectation);
  document.getElementById('run-btn').addEventListener('click', runScenario);
  document.getElementById('abort-btn').addEventListener('click', abortRun);
  document.getElementById('r-run-again').addEventListener('click', runScenario);
  document.getElementById('r-modify').addEventListener('click', () => {
    if (lastManifest) {
      document.getElementById('field-name').value = lastManifest.test_name;
      document.getElementById('field-target').value = lastManifest.target.id;
      document.getElementById('field-duration').value = lastManifest.duration_s;
    }
    renderPerturbations();
    renderExpectations();
    showStep(1);
  });

  document.getElementById('perturbation-list').addEventListener('click', e => {
    const btn = e.target.closest('.remove-pert');
    if (btn) removePerturbation(parseInt(btn.dataset.idx));
  });

  document.getElementById('perturbation-list').addEventListener('change', e => {
    const sel = e.target.closest('.pert-type');
    if (sel) updatePerturbationType(parseInt(sel.dataset.idx), sel.value);
  });

  document.getElementById('perturbation-list').addEventListener('input', e => {
    const inp = e.target.closest('.pert-param');
    if (inp) updatePerturbationParam(parseInt(inp.dataset.idx), inp.dataset.key, inp.value);
  });

  document.getElementById('expectation-list').addEventListener('click', e => {
    const btn = e.target.closest('.remove-expect');
    if (btn) {
      const row = btn.closest('.expectation-row');
      if (row) {
        const sel = row.querySelector('.expect-type');
        if (sel) removeExpectation(parseInt(sel.dataset.idx));
      }
    }
  });

  document.getElementById('expectation-list').addEventListener('change', e => {
    const sel = e.target.closest('.expect-type');
    if (sel) {
      const idx = parseInt(sel.dataset.idx);
      expectations[idx].type = sel.value;
      expectations[idx].parameters = {};
      renderExpectations();
    }
  });

  document.getElementById('expectation-list').addEventListener('input', readExpectations);

  loadTargets();
  loadLimits();
  initTheme();
  addPerturbation();
  addExpectation();
})();

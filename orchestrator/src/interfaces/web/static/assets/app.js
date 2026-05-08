(() => {
  // ── State ──
  let targets = [];
  let limits = { cpu_cores: '--', memory_total_mb: '--' };
  let perturbations = [];
  let expectations = [];
  let eventSource = null;
  let runActive = false;

  const PERTURBATION_TYPES = [
    { type: 'kill', label: 'Kill', params: [{ key: 'signal', label: 'Signal', placeholder: 'SIGKILL' }] },
    { type: 'memory_cap', label: 'Memory Cap', params: [{ key: 'limit_mb', label: 'Limit (MB)', placeholder: '64' }] },
    { type: 'cpu_cap', label: 'CPU Cap', params: [{ key: 'percent', label: 'Percent', placeholder: '50' }] },
    { type: 'network_delay', label: 'Network Delay', params: [{ key: 'delay_ms', label: 'Delay (ms)', placeholder: '1000' }] },
    { type: 'network_cutoff', label: 'Network Cutoff', params: [] },
    { type: 'garbage_packet', label: 'Garbage Packet', params: [{ key: 'count', label: 'Count', placeholder: '10' }] },
  ];

  // ── Navigation ──
  document.querySelectorAll('.nav-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
      btn.classList.add('active');
      document.getElementById('view-' + btn.dataset.view).classList.add('active');
    });
  });

  // ── Target Loading ──
  const loadTargets = async () => {
    try {
      const res = await fetch('/api/targets');
      if (!res.ok) throw new Error('Failed to load targets');
      targets = await res.json();
      renderTargets();
      populateTargetSelect();
      document.getElementById('status-badge').textContent = 'Online';
      document.getElementById('status-badge').style.color = 'var(--green)';
    } catch (err) {
      document.getElementById('status-badge').textContent = 'Offline';
      document.getElementById('status-badge').style.color = 'var(--red)';
    }
  };

  const renderTargets = () => {
    const grid = document.getElementById('target-grid');
    if (!targets.length) {
      grid.innerHTML = '<div class="empty-state">No containers detected.</div>';
      return;
    }
    grid.innerHTML = targets.map(t => {
      const state = (t.state || 'unknown').toLowerCase();
      return `<div class="target-card">
        <div class="name">${escapeHtml(t.name || '(unnamed)')}</div>
        <div class="id">${escapeHtml(t.id || '--')}</div>
        <div class="state ${state}">${t.state || 'unknown'}</div>
      </div>`;
    }).join('');
  };

  const populateTargetSelect = () => {
    const sel = document.getElementById('field-target');
    sel.innerHTML = targets.map(t =>
      `<option value="${escapeHtml(t.id)}">${escapeHtml(t.name || t.id)}</option>`
    ).join('');
  };

  // ── Limits Loading ──
  const loadLimits = async () => {
    try {
      const res = await fetch('/api/limits');
      if (!res.ok) throw new Error('Failed to load limits');
      limits = await res.json();
      document.getElementById('limit-cpu').textContent = limits.cpu_cores ?? '--';
      document.getElementById('limit-mem').textContent = limits.memory_total_mb != null ? limits.memory_total_mb + ' MB' : '--';
    } catch (err) {
      // keep defaults
    }
  };

  // ── Perturbation Management ──
  const renderPerturbations = () => {
    const container = document.getElementById('perturbation-list');
    if (!perturbations.length) {
      container.innerHTML = '<div class="empty-state" style="padding:12px;font-size:12px;">No perturbations added.</div>';
      return;
    }
    container.innerHTML = perturbations.map((p, i) => {
      const info = PERTURBATION_TYPES.find(t => t.type === p.type) || PERTURBATION_TYPES[0];
      const paramHtml = info.params.map(pr => {
        const val = p.params[pr.key] || '';
        return `<input class="pert-param" data-idx="${i}" data-key="${pr.key}" placeholder="${pr.placeholder}" value="${escapeHtml(val)}">`;
      }).join(' ');
      return `<div class="perturbation-row">
        <select class="pert-type" data-idx="${i}">
          ${PERTURBATION_TYPES.map(t =>
            `<option value="${t.type}" ${t.type === p.type ? 'selected' : ''}>${t.label}</option>`
          ).join('')}
        </select>
        ${paramHtml}
        <button class="btn xs ghost remove-pert" data-idx="${i}">✕</button>
      </div>`;
    }).join('');
  };

  const addPerturbation = () => {
    perturbations.push({ type: 'kill', params: {} });
    renderPerturbations();
  };

  const removePerturbation = (idx) => {
    perturbations.splice(idx, 1);
    renderPerturbations();
  };

  const updatePerturbationType = (idx, type) => {
    perturbations[idx].type = type;
    perturbations[idx].params = {};
    renderPerturbations();
  };

  const updatePerturbationParam = (idx, key, value) => {
    perturbations[idx].params[key] = value;
  };

  document.getElementById('add-perturbation').addEventListener('click', addPerturbation);

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

  // ── Expectation Management ──
  const readExpectations = () => {
    const rows = document.querySelectorAll('#expectation-list .expectation-row');
    expectations = [];
    rows.forEach(row => {
      const typeEl = row.querySelector('.expect-type');
      const paramEl = row.querySelector('.expect-param');
      if (typeEl && paramEl) {
        expectations.push({ type: typeEl.value, value: paramEl.value });
      }
    });
  };

  const renderExpectations = () => {
    const container = document.getElementById('expectation-list');
    container.innerHTML = expectations.map((e, i) =>
      `<div class="expectation-row">
        <select class="expect-type">
          <option value="container_running" ${e.type === 'container_running' ? 'selected' : ''}>Container Running</option>
          <option value="container_not_running" ${e.type === 'container_not_running' ? 'selected' : ''}>Container Not Running</option>
          <option value="log_contains" ${e.type === 'log_contains' ? 'selected' : ''}>Log Contains</option>
          <option value="http_status" ${e.type === 'http_status' ? 'selected' : ''}>HTTP Status</option>
        </select>
        <input class="expect-param" placeholder="value (e.g. 200)" value="${escapeHtml(e.value || '')}">
        <button class="btn xs ghost remove-expect">✕</button>
      </div>`
    ).join('');
  };

  const addExpectation = () => {
    expectations.push({ type: 'container_running', value: '' });
    renderExpectations();
  };

  document.getElementById('add-expectation').addEventListener('click', addExpectation);

  document.getElementById('expectation-list').addEventListener('click', e => {
    const btn = e.target.closest('.remove-expect');
    if (btn) {
      const row = btn.closest('.expectation-row');
      const idx = Array.from(row.parentNode.children).indexOf(row);
      expectations.splice(idx, 1);
      renderExpectations();
    }
  });

  document.getElementById('expectation-list').addEventListener('change', () => readExpectations());
  document.getElementById('expectation-list').addEventListener('input', () => readExpectations());

  // ── Build Manifest ──
  const buildManifest = () => {
    readExpectations();
    const perturbationSpecs = perturbations.map(p => {
      const spec = { type: p.type };
      Object.keys(p.params).forEach(k => {
        const val = p.params[k];
        if (val !== '') spec[k] = isNaN(val) ? val : Number(val);
      });
      return spec;
    });
    return {
      test_name: document.getElementById('field-name').value || 'Untitled Test',
      target: { id: document.getElementById('field-target').value },
      duration_s: parseInt(document.getElementById('field-duration').value) || 10,
      perturbations: perturbationSpecs,
      expectations: expectations.map(e => {
        const spec = { type: e.type };
        if (e.value !== '') spec.value = isNaN(e.value) ? e.value : Number(e.value);
        return spec;
      }),
    };
  };

  // ── Run Flow ──
  const runScenario = async () => {
    if (runActive) return;
    const manifest = buildManifest();
    const monitorPanel = document.getElementById('monitor-panel');
    const runBtn = document.getElementById('run-btn');
    const statusChip = document.getElementById('run-status-chip');
    const resultsPanel = document.getElementById('results-panel');
    const resultList = document.getElementById('result-list');
    const monLogs = document.getElementById('mon-logs');
    const monProgress = document.getElementById('mon-progress');

    resultsPanel.style.display = 'none';
    resultList.innerHTML = '';
    monLogs.textContent = 'Starting scenario...';
    monProgress.value = 0;
    statusChip.textContent = 'Pending';
    statusChip.className = 'chip pending';
    monitorPanel.style.display = 'flex';
    runBtn.disabled = true;
    runActive = true;

    if (eventSource) { eventSource.close(); eventSource = null; }

    try {
      const res = await fetch('/api/run', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(manifest),
      });
      if (!res.ok) {
        const err = await res.json().catch(() => ({}));
        throw new Error(err.error || `HTTP ${res.status}`);
      }

      statusChip.textContent = 'Running';
      statusChip.className = 'chip running';

      const duration = manifest.duration_s || 10;
      const startTime = Date.now();

      eventSource = new EventSource('/events');

      eventSource.addEventListener('state', e => {
        try {
          const data = JSON.parse(e.data);
          document.getElementById('mon-status').textContent = data.status || '--';
          document.getElementById('mon-cpu').textContent = data.cpu_usage_percent != null ? data.cpu_usage_percent + '%' : '--';
          document.getElementById('mon-mem').textContent = data.memory_usage_mb != null ? data.memory_usage_mb + ' MB' : '--';

          const elapsed = (Date.now() - startTime) / 1000;
          const pct = Math.min(Math.round((elapsed / duration) * 100), 99);
          monProgress.value = pct;

          if (data.recent_logs && data.recent_logs.length) {
            const logLine = data.recent_logs[data.recent_logs.length - 1];
            monLogs.textContent += '\n' + logLine;
            monLogs.scrollTop = monLogs.scrollHeight;
          }
        } catch (err) { /* ignore malformed state */ }
      });

      eventSource.addEventListener('complete', e => {
        try {
          const data = JSON.parse(e.data);
          const passed = data.passed === true;
          statusChip.textContent = passed ? 'PASS' : 'FAIL';
          statusChip.className = 'chip ' + (passed ? 'pass' : 'fail');
          monProgress.value = 100;

          if (data.results && data.results.length) {
            resultList.innerHTML = data.results.map(r =>
              `<li class="result-item ${r.passed ? 'pass' : 'fail'}">
                <span class="result-icon">${r.passed ? '✓' : '✗'}</span>
                <span class="result-type">${escapeHtml(r.type || '')}</span>
                <span class="result-msg">${escapeHtml(r.message || '')}</span>
              </li>`
            ).join('');
          }
          resultsPanel.style.display = 'block';
          monLogs.textContent += '\n--- Scenario complete ---';
        } catch (err) { /* ignore malformed complete */ }
        cleanupRun();
      });

      eventSource.addEventListener('error', e => {
        let errorMsg = 'Unknown error';
        try { const d = JSON.parse(e.data); errorMsg = d.error || errorMsg; } catch (_) {}
        statusChip.textContent = 'ERROR';
        statusChip.className = 'chip error';
        monLogs.textContent += '\n[ERROR] ' + errorMsg;
        cleanupRun();
      });
    } catch (err) {
      statusChip.textContent = 'ERROR';
      statusChip.className = 'chip error';
      monLogs.textContent = '[ERROR] ' + err.message;
      cleanupRun();
    }
  };

  const cleanupRun = () => {
    runActive = false;
    document.getElementById('run-btn').disabled = false;
    if (eventSource) {
      eventSource.close();
      eventSource = null;
    }
  };

  document.getElementById('run-btn').addEventListener('click', runScenario);

  // ── Refresh ──
  document.getElementById('refresh-targets').addEventListener('click', () => {
    loadTargets();
    loadLimits();
  });

  // ── Helpers ──
  const escapeHtml = (str) => {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  };

  // ── Init ──
  loadTargets();
  loadLimits();
  addPerturbation();
})();

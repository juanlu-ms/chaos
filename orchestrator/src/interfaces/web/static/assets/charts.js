'use strict';

function createLiveChart(canvasId, label, unit, colorVar) {
    const ctx = document.getElementById(canvasId)?.getContext('2d');
    if (!ctx) return null;
    return new Chart(ctx, {
        type: 'line',
        data: { datasets: [{ label, data: [], borderColor: `var(${colorVar})`,
            backgroundColor: `var(${colorVar})` + '33', fill: true, tension: 0.2,
            pointRadius: 0, borderWidth: 2 }] },
        options: {
            responsive: true, maintainAspectRatio: false,
            animation: { duration: 300 },
            scales: {
                x: { type: 'time', time: { displayFormats: { second: ':mm:ss' } },
                    ticks: { maxTicksLimit: 10, color: getCSS('--text-dim') },
                    grid: { color: getCSS('--border') + '44' } },
                y: { beginAtZero: true, ticks: { color: getCSS('--text-dim') },
                    grid: { color: getCSS('--border') + '44' },
                    title: { display: true, text: unit, color: getCSS('--text-dim') } },
            },
            plugins: { legend: { display: false } },
        },
    });
}

function createResultsChart(canvasId, label, unit, colorVar) {
    const ctx = document.getElementById(canvasId)?.getContext('2d');
    if (!ctx) return null;
    return new Chart(ctx, {
        type: 'line',
        data: { datasets: [{ label, data: [], borderColor: `var(${colorVar})`,
            backgroundColor: `var(${colorVar})` + '22', fill: true, tension: 0.1,
            pointRadius: 1, pointBorderColor: `var(${colorVar})`,
            pointBackgroundColor: `var(${colorVar})`, borderWidth: 2 }] },
        options: {
            responsive: true, maintainAspectRatio: false, animation: false,
            scales: {
                x: { type: 'time', time: { displayFormats: { second: ':mm:ss' } },
                    ticks: { maxTicksLimit: 15, color: getCSS('--text-dim') },
                    grid: { color: getCSS('--border') + '44' } },
                y: { beginAtZero: true, ticks: { color: getCSS('--text-dim') },
                    grid: { color: getCSS('--border') + '44' },
                    title: { display: true, text: unit, color: getCSS('--text-dim') } },
            },
            plugins: { legend: { display: false } },
        },
        plugins: [zoneBackgroundPlugin],
    });
}

function pushChartData(chart, timestamp, value) {
    if (!chart) return;
    const max = 60;
    chart.data.datasets[0].data.push({ x: timestamp, y: value });
    if (chart.data.datasets[0].data.length > max) chart.data.datasets[0].data.shift();
    chart.update('none');
}

function setResultsData(chart, dataPoints) {
    if (!chart) return;
    chart.data.datasets[0].data = dataPoints;
    chart.update('none');
}

function getCSS(v) {
    return getComputedStyle(document.documentElement).getPropertyValue(v).trim() || '#888';
}

const zoneBackgroundPlugin = {
    id: 'zoneBackground',
    beforeDraw(chart) {
        const { ctx, chartArea, scales, data } = chart;
        if (!chartArea || !data.datasets[0].data.length) return;
        const points = data.datasets[0].data;
        const successC = getCSS('--success-muted') || 'rgba(76,175,80,0.08)';
        const errorC = getCSS('--error-muted') || 'rgba(239,83,80,0.08)';
        let chaosStart = null, recoveryStart = null, lastPhase = 'normal';
        points.forEach(pt => {
            const ph = pt.phase || lastPhase;
            if (ph === 'chaos' && lastPhase === 'normal') chaosStart = pt.x;
            if (ph === 'recovery' && lastPhase === 'chaos') recoveryStart = pt.x;
            lastPhase = ph;
        });
        const xAxis = scales.x;
        const draw = (from, to, color) => {
            const x1 = xAxis.getPixelForValue(from);
            const x2 = to ? xAxis.getPixelForValue(to) : chartArea.right;
            ctx.fillStyle = color;
            ctx.fillRect(Math.max(x1, chartArea.left), chartArea.top,
                Math.min(x2, chartArea.right) - Math.max(x1, chartArea.left),
                chartArea.bottom - chartArea.top);
        };
        if (chaosStart) draw(points[0].x, chaosStart, successC);
        if (chaosStart && recoveryStart) draw(chaosStart, recoveryStart, errorC);
        if (recoveryStart) draw(recoveryStart, null, successC);
    },
};

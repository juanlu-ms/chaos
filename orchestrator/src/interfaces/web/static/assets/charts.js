class BarChart {
    constructor(canvasId, options = {}) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        this.maxPoints = options.maxPoints || 60;
        this.data = [];
        this.peak = 0;
        this.unit = options.unit || '';
        this.color = options.color || 'var(--accent)';
        this.resize();
        this.resizeHandler = () => this.resize();
        window.addEventListener('resize', this.resizeHandler);
    }
    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        this.canvas.width = rect.width - 4;
        this.canvas.height = rect.height - 4;
        this.draw();
    }
    push(value) {
        this.data.push(value);
        if (this.data.length > this.maxPoints) this.data.shift();
        this.peak = this.data.reduce((a, b) => Math.max(a, b), 0);
        this.draw();
    }
    clear() { this.data = []; this.peak = 0; this.draw(); }
    draw() {
        const ctx = this.ctx, w = this.canvas.width, h = this.canvas.height;
        if (!w || !h) return;
        const max = this.peak || 1;
        const bw = Math.max(3, (w / this.maxPoints) - 1);
        ctx.clearRect(0, 0, w, h);
        const accentColor = getComputedStyle(document.documentElement).getPropertyValue('--accent').trim() || '#e6a817';
        this.data.forEach((val, i) => {
            if (val === null || val === undefined) return;
            const x = i * (bw + 1);
            const bh = Math.max(2, (val / max) * (h - 4));
            const grad = ctx.createLinearGradient(0, h - 4 - bh, 0, h - 4);
            grad.addColorStop(0, accentColor);
            grad.addColorStop(1, 'transparent');
            ctx.fillStyle = grad;
            ctx.fillRect(x, h - 4 - bh, bw, bh);
        });
        ctx.strokeStyle = 'rgba(255,255,255,0.06)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, h - 4);
        ctx.lineTo(w, h - 4);
        ctx.stroke();
    }
    destroy() {
        window.removeEventListener('resize', this.resizeHandler);
    }
}

class TimelineChart extends BarChart {
    constructor(canvasId, options = {}) {
        super(canvasId, options);
        this.chaosStart = -1;
        this.recoveryStart = -1;
    }
    setZones(chaosStart, recoveryStart) {
        this.chaosStart = chaosStart;
        this.recoveryStart = recoveryStart;
        this.draw();
    }
    draw() {
        const ctx = this.ctx, w = this.canvas.width, h = this.canvas.height;
        if (!w || !h) return;
        const max = this.peak || 1;
        const len = this.data.length;
        const bw = Math.max(3, (w / this.maxPoints) - 1);
        ctx.clearRect(0, 0, w, h);
        const styles = getComputedStyle(document.documentElement);
        if (this.chaosStart >= 0 && len > 0) {
            const normalColor = styles.getPropertyValue('--success-muted').trim() || 'rgba(76,175,80,0.08)';
            const chaosColor = styles.getPropertyValue('--error-muted').trim() || 'rgba(239,83,80,0.08)';
            const recoveryColor = styles.getPropertyValue('--success-muted').trim() || 'rgba(76,175,80,0.08)';
            const total = w;
            const chaosX = (this.chaosStart / this.maxPoints) * total;
            const recoveryX = (this.recoveryStart / this.maxPoints) * total;
            ctx.fillStyle = normalColor;
            ctx.fillRect(0, 0, chaosX, h);
            ctx.fillStyle = chaosColor;
            ctx.fillRect(chaosX, 0, recoveryX - chaosX, h);
            ctx.fillStyle = recoveryColor;
            ctx.fillRect(recoveryX, 0, total - recoveryX, h);
        }
        const normalColor = styles.getPropertyValue('--success').trim() || '#4caf50';
        const chaosColor = styles.getPropertyValue('--error').trim() || '#ef5350';
        const recoveryColor = styles.getPropertyValue('--success').trim() || '#4caf50';
        this.data.forEach((val, i) => {
            if (val === null || val === undefined) return;
            const x = i * (bw + 1);
            const bh = Math.max(2, (val / max) * (h - 4));
            let color = normalColor;
            if (this.chaosStart >= 0 && i >= this.chaosStart) color = chaosColor;
            if (this.recoveryStart >= 0 && i >= this.recoveryStart) color = recoveryColor;
            ctx.fillStyle = color;
            ctx.fillRect(x, h - 4 - bh, bw, bh);
        });
        ctx.strokeStyle = 'rgba(255,255,255,0.06)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, h - 4);
        ctx.lineTo(w, h - 4);
        ctx.stroke();
    }
}

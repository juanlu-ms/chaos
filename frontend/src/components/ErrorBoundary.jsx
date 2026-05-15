import { Component } from 'react';

export default class ErrorBoundary extends Component {
  constructor(props) {
    super(props);
    this.state = { error: null };
  }

  static getDerivedStateFromError(error) {
    return { error };
  }

  componentDidCatch(error, info) {
    // eslint-disable-next-line no-console
    console.error('[ErrorBoundary] render error:', error, info?.componentStack);
  }

  reset = () => {
    this.setState({ error: null });
    this.props.onReset?.();
  };

  render() {
    const { error } = this.state;
    if (!error) return this.props.children;
    return (
      <div className="panel" role="alert" style={{ margin: 16 }}>
        <h3 style={{ color: 'var(--error, #ef5350)' }}>Something went wrong</h3>
        <pre style={{
          whiteSpace: 'pre-wrap', fontSize: 12, color: 'var(--text-dim)',
          maxHeight: 240, overflow: 'auto',
        }}>{String(error?.stack || error?.message || error)}</pre>
        <button onClick={this.reset} style={{ marginTop: 8 }}>Reset view</button>
      </div>
    );
  }
}

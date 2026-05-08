import { THEMES } from '../theme.js';

export default function Topbar({
  online,
  currentStep,
  setStep,
  theme,
  setTheme,
  runActive,
}) {
  const tabs = [
    { n: 1, label: 'Configure' },
    { n: 2, label: 'Monitor' },
    { n: 3, label: 'Results' },
  ];

  return (
    <div className="topbar">
      <div className="logo">CHAOS</div>
      <div className={`status-badge ${online ? 'online' : 'offline'}`}>
        <span className="dot" />
        {online ? 'Online' : 'Offline'}
      </div>

      <div className="steps">
        {tabs.map((t) => {
          const isActive = currentStep === t.n;
          // While a run is active (step 2), disable steps 1 and 3.
          const disabled = runActive && t.n !== 2;
          return (
            <button
              key={t.n}
              className={`step-tab ${isActive ? 'active' : ''} ${
                disabled ? 'disabled' : ''
              }`}
              onClick={() => !disabled && setStep(t.n)}
            >
              {t.n}. {t.label}
            </button>
          );
        })}
      </div>

      <select
        className="theme-select"
        value={theme}
        onChange={(e) => setTheme(e.target.value)}
        aria-label="Theme"
      >
        {THEMES.map((t) => (
          <option key={t} value={t}>
            {t[0].toUpperCase() + t.slice(1)}
          </option>
        ))}
      </select>
    </div>
  );
}

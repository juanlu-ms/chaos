import { THEMES } from '../theme.js';
import Logo from './Logo.jsx';

export default function Topbar({
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
      <Logo />

      <div className="steps" role="tablist" aria-label="Workflow steps">
        {tabs.map((t) => {
          const isActive = currentStep === t.n;
          // While a run is active, only the Monitor tab is reachable.
          const disabled = runActive && t.n !== 2;
          return (
            <button
              key={t.n}
              role="tab"
              aria-selected={isActive}
              aria-disabled={disabled}
              tabIndex={disabled ? -1 : 0}
              disabled={disabled}
              className={`step-tab ${isActive ? 'active' : ''} ${disabled ? 'disabled' : ''}`}
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

import { useEffect, useState } from 'react';
import Topbar from './components/Topbar.jsx';
import Step1Config from './steps/Step1Config.jsx';
import Step2Monitor from './steps/Step2Monitor.jsx';
import Step3Results from './steps/Step3Results.jsx';
import { runManifest, getTargets } from './api.js';

export default function App() {
  const [theme, setTheme] = useState(
    () => localStorage.getItem('chaos-theme') || 'amber'
  );
  const [step, setStep] = useState(1);
  const [manifest, setManifest] = useState(null);
  const [testStartTime, setTestStartTime] = useState(0);
  const [result, setResult] = useState(null);
  const [online, setOnline] = useState(false);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    localStorage.setItem('chaos-theme', theme);
  }, [theme]);

  // Periodic health ping for the topbar status badge.
  useEffect(() => {
    let cancelled = false;
    const ping = () => {
      getTargets()
        .then(() => !cancelled && setOnline(true))
        .catch(() => !cancelled && setOnline(false));
    };
    ping();
    const id = setInterval(ping, 5000);
    return () => {
      cancelled = true;
      clearInterval(id);
    };
  }, []);

  const handleRun = (m) => {
    setManifest(m);
    setTestStartTime(Date.now());
    setResult(null);
    setStep(2);
  };

  const handleComplete = (r) => {
    setResult(r);
    setStep(3);
  };

  const handleRunAgain = async () => {
    if (!manifest) return;
    try {
      await runManifest(manifest);
      setTestStartTime(Date.now());
      setResult(null);
      setStep(2);
    } catch (e) {
      alert(`Failed to start: ${e.message}`);
    }
  };

  const handleModify = () => {
    setStep(1);
  };

  const handleBackToStep1 = () => {
    setStep(1);
  };

  return (
    <div className="app">
      <Topbar
        online={online}
        currentStep={step}
        setStep={setStep}
        theme={theme}
        setTheme={setTheme}
        runActive={step === 2}
      />
      <main>
        {step === 1 && (
          <Step1Config
            initial={manifest}
            onRun={handleRun}
            setOnline={setOnline}
          />
        )}
        {step === 2 && (
          <Step2Monitor
            manifest={manifest}
            testStartTime={testStartTime}
            theme={theme}
            onComplete={handleComplete}
            onBack={handleBackToStep1}
          />
        )}
        {step === 3 && (
          <Step3Results
            manifest={manifest}
            result={result}
            theme={theme}
            onRunAgain={handleRunAgain}
            onModify={handleModify}
          />
        )}
      </main>
    </div>
  );
}

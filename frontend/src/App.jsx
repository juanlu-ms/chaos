import { useEffect, useRef, useState } from 'react';
import Topbar from './components/Topbar.jsx';
import Step1Config from './steps/Step1Config.jsx';
import Step2Monitor from './steps/Step2Monitor.jsx';
import Step3Results from './steps/Step3Results.jsx';
import { runManifest, abortRun } from './api.js';
import { useSSEStream } from './hooks/useSSEStream.js';
import { useStreamData } from './hooks/useStreamData.js';

export default function App() {
  const [theme, setTheme] = useState(
    () => localStorage.getItem('chaos-theme') || 'amber'
  );
  const [step, setStep] = useState(1);
  const [manifest, setManifest] = useState(null);
  const [testStartTime, setTestStartTime] = useState(0);
  const [result, setResult] = useState(null);
  const [isRunning, setIsRunning] = useState(false);

  const { startStream, closeStream } = useSSEStream();
  const {
    data, logs, zones, lastState, conn,
    resetData, handleState, handleConnectionError, setConn
  } = useStreamData();

  // Refs that always see latest state for SSE callbacks.
  const dataRef = useRef(data);
  const logsRef = useRef(logs);
  const zonesRef = useRef(zones);
  const lastStateRef = useRef(lastState);
  useEffect(() => { dataRef.current = data; }, [data]);
  useEffect(() => { logsRef.current = logs; }, [logs]);
  useEffect(() => { zonesRef.current = zones; }, [zones]);
  useEffect(() => { lastStateRef.current = lastState; }, [lastState]);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    localStorage.setItem('chaos-theme', theme);
  }, [theme]);

  const handleRun = (m) => {
    setManifest(m);
    const start = Date.now();
    setTestStartTime(start);
    resetData();
    setResult(null);
    setIsRunning(true);

    startStream(start, {
      onState: (s) => handleState(s, start),
      onComplete: (payload) => {
        setIsRunning(false);
        setResult({
          ...payload,
          data: dataRef.current,
          logs: logsRef.current,
          zones: zonesRef.current,
          lastState: lastStateRef.current,
        });
        setStep(3);
      },
      onServerError: (payload) => {
        setIsRunning(false);
        setResult({
          passed: false,
          error: payload.error,
          results: [],
          data: dataRef.current,
          logs: logsRef.current,
          zones: zonesRef.current,
          lastState: lastStateRef.current,
        });
        setStep(3);
      },
      onConnectionError: handleConnectionError,
    });
    setStep(2);
  };

  const handleRunAgain = async () => {
    if (!manifest) return;
    try {
      await runManifest(manifest);
      const start = Date.now();
      setTestStartTime(start);
      resetData();
      setResult(null);
      setIsRunning(true);

      startStream(start, {
        onState: (s) => handleState(s, start),
        onComplete: (payload) => {
          setIsRunning(false);
          setResult({
            ...payload,
            data: dataRef.current,
            logs: logsRef.current,
            zones: zonesRef.current,
            lastState: lastStateRef.current,
          });
          setStep(3);
        },
        onServerError: (payload) => {
          setIsRunning(false);
          setResult({
            passed: false,
            error: payload.error,
            results: [],
            data: dataRef.current,
            logs: logsRef.current,
            zones: zonesRef.current,
            lastState: lastStateRef.current,
          });
          setStep(3);
        },
        onConnectionError: handleConnectionError,
      });
      setStep(2);
    } catch (e) {
      alert(`Failed to start: ${e.message}`);
    }
  };

  const handleAbort = async () => {
    try { await abortRun(); } catch (e) { console.error(e); }
  };

  const handleModify = () => setStep(1);
  const handleBackToStep1 = () => setStep(1);

  return (
    <div className="app">
      <Topbar
        currentStep={step}
        setStep={setStep}
        theme={theme}
        setTheme={setTheme}
        runActive={isRunning}
      />
      <main>
        {step === 1 && (
          <Step1Config initial={manifest} onRun={handleRun} />
        )}
        {step === 2 && (
          <Step2Monitor
            manifest={manifest}
            testStartTime={testStartTime}
            theme={theme}
            data={data}
            logs={logs}
            lastState={lastState}
            conn={conn}
            isRunning={isRunning}
            onAbort={handleAbort}
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

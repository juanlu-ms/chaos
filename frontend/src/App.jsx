import { useEffect, useRef, useState } from 'react';
import Topbar from './components/Topbar.jsx';
import Step1Config from './steps/Step1Config.jsx';
import Step2Monitor from './steps/Step2Monitor.jsx';
import Step3Results from './steps/Step3Results.jsx';
import { runManifest, abortRun, openEventStream } from './api.js';

export default function App() {
  const [theme, setTheme] = useState(
    () => localStorage.getItem('chaos-theme') || 'amber'
  );
  const [step, setStep] = useState(1);
  const [manifest, setManifest] = useState(null);
  const [testStartTime, setTestStartTime] = useState(0);
  const [result, setResult] = useState(null);
  const [isRunning, setIsRunning] = useState(false);

  // Live monitor state lifted into App so Monitor/Results survive tab nav.
  const [data, setData] = useState([]);
  const [logs, setLogs] = useState([]);
  const [zones, setZones] = useState({ normalEnd: 0, chaosEnd: 0 });
  const [lastState, setLastState] = useState(null);
  const [conn, setConn] = useState(null); // null | 'lost' | 'dead'

  const phaseRef = useRef('normal');
  const lastEventAt = useRef(Date.now());
  const lostTimer = useRef(null);
  const closeRef = useRef(null);
  const startRef = useRef(0);
  const runEpoch = useRef(0);

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

  const closeStream = () => {
    if (closeRef.current) {
      closeRef.current();
      closeRef.current = null;
    }
    if (lostTimer.current) {
      clearTimeout(lostTimer.current);
      lostTimer.current = null;
    }
  };

  const startStream = (start) => {
    const epoch = ++runEpoch.current;
    closeStream();
    phaseRef.current = 'normal';
    startRef.current = start;
    setData([]);
    setLogs([]);
    setZones({ normalEnd: 0, chaosEnd: 0 });
    setLastState(null);
    setConn(null);
    setResult(null);
    setIsRunning(true);

    closeRef.current = openEventStream({
      onState: (s) => {
        if (runEpoch.current !== epoch) return;
        lastEventAt.current = Date.now();
        setConn((c) => (c ? null : c));
        const t = (Date.now() - startRef.current) / 1000;
        const net = (s.network_rx_bps || 0) + (s.network_tx_bps || 0);
        const backendT = s.backend_elapsed_ms != null ? s.backend_elapsed_ms / 1000 : null;
        setData((prev) => [
          ...prev,
          {
            t,
            cpu: s.cpu_usage_percent ?? 0,
            mem: s.memory_usage_mb ?? 0,
            net,
            phase: s.phase || 'normal',
            backendT,
          },
        ]);
        setLastState(s);
        // Debug timing: compare frontend clock vs backend clock
        if (backendT != null) {
          console.log(`[timing] frontend= ${t.toFixed(1)}s  backend= ${backendT.toFixed(1)}s  drift= ${(t - backendT).toFixed(1)}s  phase= ${s.phase}`);
        } else {
          console.log(`[timing] frontend= ${t.toFixed(1)}s  backend= MISSING  phase= ${s.phase}`);
        }
        if (s.recent_logs && s.recent_logs.length) {
          setLogs((prev) => {
            const seen = new Set(prev);
            const merged = [...prev];
            for (const line of s.recent_logs) {
              if (line && !seen.has(line)) {
                merged.push(line);
                seen.add(line);
              }
            }
            return merged;
          });
        }
        const prev = phaseRef.current;
        const next = s.phase || 'normal';
        if (prev !== next) {
          if (prev === 'normal' && next === 'chaos') {
            setZones((z) => ({ ...z, normalEnd: t }));
          } else if (prev === 'chaos' && next === 'recovery') {
            setZones((z) => ({ ...z, chaosEnd: t }));
          }
          phaseRef.current = next;
        }
      },
      onComplete: (payload) => {
        if (runEpoch.current !== epoch) return;
        closeStream();
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
        if (runEpoch.current !== epoch) return;
        closeStream();
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
      onConnectionError: () => {
        setConn('lost');
        if (lostTimer.current) clearTimeout(lostTimer.current);
        lostTimer.current = setTimeout(() => {
          if (Date.now() - lastEventAt.current >= 9500) setConn('dead');
        }, 10000);
      },
      onConnectionRestored: () => {},
    });
  };

  useEffect(() => () => closeStream(), []);

  const handleRun = (m) => {
    setManifest(m);
    const start = Date.now();
    setTestStartTime(start);
    startStream(start);
    setStep(2);
  };

  const handleRunAgain = async () => {
    if (!manifest) return;
    try {
      await runManifest(manifest);
      const start = Date.now();
      setTestStartTime(start);
      startStream(start);
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

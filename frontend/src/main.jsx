import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App.jsx';
import { DEFAULT_THEME } from './theme.js';
import './styles.css';

const saved = localStorage.getItem('chaos-theme') || DEFAULT_THEME;
document.documentElement.dataset.theme = saved;

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

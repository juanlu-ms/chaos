import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

const BACKEND = 'http://127.0.0.1:8080';
const proxy = ['/api', '/events', '/containers'].reduce((acc, path) => {
  acc[path] = { target: BACKEND, changeOrigin: true };
  return acc;
}, {});

export default defineConfig({
  plugins: [react()],
  base: '/',
  build: {
    outDir: '../orchestrator/src/interfaces/web/static',
    emptyOutDir: true,
  },
  rolldownOptions: {
    output: {
      manualChunks: {
        // Crea un archivo separado para las librerías pesadas
        'vendor-ui': ['react', 'react-dom'],
        'vendor-charts': ['recharts'],
      },
    },
  },
  server: { proxy },
});

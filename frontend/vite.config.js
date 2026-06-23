import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

const BACKEND = 'http://127.0.0.1:8080';
const proxy = ['/api'].reduce((acc, path) => {
  acc[path] = { target: BACKEND, changeOrigin: true };
  return acc;
}, {});

export default defineConfig({
  plugins: [react()],
  base: '/',
  build: {
    outDir: process.env.CHAOS_WEB_OUT_DIR || 'dist',
    emptyOutDir: true,
  },
  server: { proxy },
});

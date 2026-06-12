import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import {
  apiProxyHostEnv,
  apiProxyPortEnv,
  apiRootPath,
  backendPortEnv,
  defaultBackendProxyHost,
  defaultBackendProxyPort,
  defaultBackendProxyProtocol,
  defaultFrontendHost,
  defaultFrontendPort,
  frontendHostEnv,
  frontendPortEnv,
} from './src/config/constants';

const envValue = (name: string) => {
  const value = process.env[name]?.trim();
  return value ? value : undefined;
};

const optionalEnvPort = (name: string) => {
  const raw = envValue(name);
  if (!raw) {
    return undefined;
  }

  const port = Number.parseInt(raw, 10);
  if (!Number.isInteger(port) || port <= 0 || port > 65535) {
    throw new Error(`${name} out of range`);
  }
  return port;
};

const frontendHost = envValue(frontendHostEnv) ?? defaultFrontendHost;
const frontendPort = optionalEnvPort(frontendPortEnv) ?? defaultFrontendPort;
const backendProxyHost = envValue(apiProxyHostEnv) ?? defaultBackendProxyHost;
const backendProxyPort =
  optionalEnvPort(apiProxyPortEnv) ?? optionalEnvPort(backendPortEnv) ?? defaultBackendProxyPort;
const backendProxyTarget = `${defaultBackendProxyProtocol}://${backendProxyHost}:${backendProxyPort}`;

const apiProxy = {
  [apiRootPath]: {
    target: backendProxyTarget,
    changeOrigin: true,
    ws: true,
  },
};

export default defineConfig({
  plugins: [react()],
  server: {
    host: frontendHost,
    port: frontendPort,
    strictPort: true,
    proxy: apiProxy,
  },
  preview: {
    host: frontendHost,
    port: frontendPort,
    strictPort: true,
    proxy: apiProxy,
  },
});

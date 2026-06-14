export const frontendHostEnv = 'SM_FRONTEND_HOST';
export const frontendPortEnv = 'SM_FRONTEND_PORT';
export const apiProxyHostEnv = 'SM_API_PROXY_HOST';
export const apiProxyPortEnv = 'SM_API_PROXY_PORT';
export const backendPortEnv = 'SM_PORT';
export const apiBaseUrlEnv = 'VITE_API_BASE_URL';
export const webSocketBaseUrlEnv = 'VITE_WS_BASE_URL';

export const defaultFrontendHost = '0.0.0.0';
export const defaultFrontendPort = 5173;
export const defaultBackendProxyProtocol = 'http';
export const defaultBackendProxyHost = '127.0.0.1';
export const defaultBackendProxyPort = 8080;

export const apiRootPath = '/api';
export const studentUploadsContextType = 'student_uploads';
export const globalTemplatesContextType = 'global_templates';
export const defaultGlobalTemplatesContextId = 'default';
export const webSocketStudentsPath = `${apiRootPath}/ws/students`;
export const webSocketStudentIdParameter = 'student_id';
export const webSocketScopeParameter = 'scope';
export const webSocketSchoolScope = 'school';

export const httpsProtocol = 'https:';
export const webSocketProtocol = 'ws:';
export const secureWebSocketProtocol = 'wss:';

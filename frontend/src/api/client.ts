import type {
  Grade,
  GradePatch,
  StoredFile,
  Student,
  StudentDetail,
  StudentInfoDefinition,
  StudentInfoDefinitionPatch,
  StudentInfoField,
  StudentInfoValuePatch,
  StudentPatch,
} from './types';
import {
  apiBaseUrlEnv,
  httpsProtocol,
  secureWebSocketProtocol,
  webSocketBaseUrlEnv,
  webSocketProtocol,
} from '../config/constants';
import { apiPaths } from './paths';

const trimTrailingSlashes = (value: string) => value.replace(/\/+$/, '');
const configuredEnv = (key: string) => {
  const value = import.meta.env[key];
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
};

export const API_BASE_URL = trimTrailingSlashes(configuredEnv(apiBaseUrlEnv) ?? '');

const webSocketProtocolFor = (protocol: string) =>
  protocol === httpsProtocol ? secureWebSocketProtocol : webSocketProtocol;

const defaultWebSocketBaseUrl = () => `${webSocketProtocolFor(window.location.protocol)}//${window.location.host}`;

const webSocketBaseUrlFromApiBaseUrl = (apiBaseUrl: string) => {
  if (!apiBaseUrl) {
    return undefined;
  }

  const url = new URL(apiBaseUrl, window.location.origin);
  url.protocol = webSocketProtocolFor(url.protocol);
  return trimTrailingSlashes(url.toString());
};

export const WS_BASE_URL = trimTrailingSlashes(
  configuredEnv(webSocketBaseUrlEnv) ?? webSocketBaseUrlFromApiBaseUrl(API_BASE_URL) ?? defaultWebSocketBaseUrl(),
);

export const apiUrl = (path: string) => `${API_BASE_URL}${path}`;
export const webSocketUrl = (path: string) => `${WS_BASE_URL}${path}`;

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(apiUrl(path), {
    ...init,
    headers: {
      ...(init?.body instanceof FormData ? {} : { 'Content-Type': 'application/json' }),
      ...init?.headers,
    },
  });

  if (!response.ok) {
    let message = `${response.status} ${response.statusText}`;
    try {
      const error = (await response.json()) as { error?: string };
      if (error.error) {
        message = error.error;
      }
    } catch {
      // Keep the HTTP status text when the server returns a non-JSON response.
    }
    throw new Error(message);
  }

  return (await response.json()) as T;
}

export async function listStudents(): Promise<Student[]> {
  const data = await request<{ students: Student[] }>(apiPaths.students());
  return data.students;
}

export async function createStudent(displayName: string): Promise<Student> {
  const data = await request<{ student: Student }>(apiPaths.students(), {
    method: 'POST',
    body: JSON.stringify({ display_name: displayName }),
  });
  return data.student;
}

export async function patchStudent(studentId: string, patch: StudentPatch): Promise<Student> {
  const data = await request<{ student: Student }>(apiPaths.student(studentId), {
    method: 'PATCH',
    body: JSON.stringify(patch),
  });
  return data.student;
}

export async function deleteStudent(studentId: string): Promise<void> {
  await request<{ deleted: boolean }>(apiPaths.student(studentId), {
    method: 'DELETE',
  });
}

export async function getStudent(studentId: string): Promise<StudentDetail> {
  return request<StudentDetail>(apiPaths.student(studentId));
}

export async function listStudentInfoDefinitions(): Promise<StudentInfoDefinition[]> {
  const data = await request<{ info_field_definitions: StudentInfoDefinition[] }>(
    apiPaths.studentInfoDefinitions(),
  );
  return data.info_field_definitions;
}

export async function createStudentInfoDefinition(
  payload: Pick<StudentInfoDefinition, 'name' | 'display_name' | 'value_type'>,
): Promise<StudentInfoDefinition> {
  const data = await request<{ info_field_definition: StudentInfoDefinition }>(
    apiPaths.studentInfoDefinitions(),
    {
      method: 'POST',
      body: JSON.stringify(payload),
    },
  );
  return data.info_field_definition;
}

export async function patchStudentInfoDefinition(
  fieldId: string,
  patch: StudentInfoDefinitionPatch,
): Promise<StudentInfoDefinition> {
  const data = await request<{ info_field_definition: StudentInfoDefinition }>(
    apiPaths.studentInfoDefinition(fieldId),
    {
      method: 'PATCH',
      body: JSON.stringify(patch),
    },
  );
  return data.info_field_definition;
}

export async function deleteStudentInfoDefinition(fieldId: string): Promise<void> {
  await request<{ deleted: boolean }>(apiPaths.studentInfoDefinition(fieldId), {
    method: 'DELETE',
  });
}

export async function patchStudentInfoValue(
  studentId: string,
  fieldId: string,
  patch: StudentInfoValuePatch,
): Promise<StudentInfoField> {
  const data = await request<{ info_field: StudentInfoField }>(
    apiPaths.studentInfoValue(studentId, fieldId),
    {
      method: 'PATCH',
      body: JSON.stringify(patch),
    },
  );
  return data.info_field;
}

export async function createGrade(studentId: string): Promise<Grade> {
  const data = await request<{ grade: Grade }>(apiPaths.grades(studentId), {
    method: 'POST',
    body: JSON.stringify({ title: 'New grade', score: '' }),
  });
  return data.grade;
}

export async function patchGrade(studentId: string, gradeId: string, patch: GradePatch): Promise<Grade> {
  const data = await request<{ grade: Grade }>(apiPaths.grade(studentId, gradeId), {
    method: 'PATCH',
    body: JSON.stringify(patch),
  });
  return data.grade;
}

export async function deleteGrade(studentId: string, gradeId: string): Promise<void> {
  await request<{ deleted: boolean }>(apiPaths.grade(studentId, gradeId), {
    method: 'DELETE',
  });
}

export function uploadFile(
  studentId: string,
  file: File,
  onProgress: (percent: number) => void,
): Promise<StoredFile> {
  const form = new FormData();
  form.append('file', file);

  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', apiUrl(apiPaths.files(studentId)));
    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) {
        onProgress(Math.round((event.loaded / event.total) * 100));
      }
    };
    xhr.onload = () => {
      if (xhr.status < 200 || xhr.status >= 300) {
        try {
          const error = JSON.parse(xhr.responseText) as { error?: string };
          reject(new Error(error.error ?? xhr.statusText));
        } catch {
          reject(new Error(xhr.statusText));
        }
        return;
      }
      const payload = JSON.parse(xhr.responseText) as { file: StoredFile };
      resolve(payload.file);
    };
    xhr.onerror = () => reject(new Error('upload failed'));
    xhr.send(form);
  });
}

export function downloadUrl(studentId: string, fileId: string): string {
  return apiUrl(apiPaths.fileDownload(studentId, fileId));
}

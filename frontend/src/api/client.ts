import type { Grade, GradePatch, StoredFile, Student, StudentDetail, StudentPatch } from './types';

export const API_BASE_URL = import.meta.env.VITE_API_BASE_URL ?? 'http://localhost:8080';

export const WS_BASE_URL =
  import.meta.env.VITE_WS_BASE_URL ??
  API_BASE_URL.replace(/^http:/, 'ws:').replace(/^https:/, 'wss:');

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${API_BASE_URL}${path}`, {
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
  const data = await request<{ students: Student[] }>('/api/students');
  return data.students;
}

export async function createStudent(displayName: string): Promise<Student> {
  const data = await request<{ student: Student }>('/api/students', {
    method: 'POST',
    body: JSON.stringify({ display_name: displayName }),
  });
  return data.student;
}

export async function patchStudent(studentId: string, patch: StudentPatch): Promise<Student> {
  const data = await request<{ student: Student }>(`/api/students/${studentId}`, {
    method: 'PATCH',
    body: JSON.stringify(patch),
  });
  return data.student;
}

export async function deleteStudent(studentId: string): Promise<void> {
  await request<{ deleted: boolean }>(`/api/students/${studentId}`, {
    method: 'DELETE',
  });
}

export async function getStudent(studentId: string): Promise<StudentDetail> {
  return request<StudentDetail>(`/api/students/${studentId}`);
}

export async function createGrade(studentId: string): Promise<Grade> {
  const data = await request<{ grade: Grade }>(`/api/students/${studentId}/grades`, {
    method: 'POST',
    body: JSON.stringify({ title: 'New grade', score: '' }),
  });
  return data.grade;
}

export async function patchGrade(studentId: string, gradeId: string, patch: GradePatch): Promise<Grade> {
  const data = await request<{ grade: Grade }>(`/api/students/${studentId}/grades/${gradeId}`, {
    method: 'PATCH',
    body: JSON.stringify(patch),
  });
  return data.grade;
}

export async function deleteGrade(studentId: string, gradeId: string): Promise<void> {
  await request<{ deleted: boolean }>(`/api/students/${studentId}/grades/${gradeId}`, {
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
    xhr.open('POST', `${API_BASE_URL}/api/students/${studentId}/files`);
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
  return `${API_BASE_URL}/api/students/${studentId}/files/${fileId}/download`;
}

import { useEffect, useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { WS_BASE_URL } from './client';
import type { Student, StudentDetail, WsMessage } from './types';

function upsertStudent(students: Student[] = [], student: Student): Student[] {
  return [student, ...students.filter((item) => item.id !== student.id)];
}

export function useSchoolSocket() {
  const queryClient = useQueryClient();
  const [status, setStatus] = useState<'idle' | 'connected' | 'reconnecting'>('idle');

  useEffect(() => {
    let socket: WebSocket | null = null;
    let closedByEffect = false;
    let retryTimer: number | undefined;

    const connect = () => {
      setStatus((current) => (current === 'idle' ? 'reconnecting' : current));
      socket = new WebSocket(`${WS_BASE_URL}/api/ws/students?scope=school`);

      socket.onopen = () => setStatus('connected');
      socket.onclose = () => {
        if (!closedByEffect) {
          setStatus('reconnecting');
          retryTimer = window.setTimeout(connect, 1000);
        }
      };
      socket.onmessage = (event) => {
        const message = JSON.parse(event.data) as WsMessage;
        if (message.resource !== 'student') {
          return;
        }

        if (message.type === 'student.created' || message.type === 'student.updated') {
          queryClient.setQueryData<Student[]>(['students'], (old) => upsertStudent(old, message.student));
          queryClient.setQueryData<StudentDetail>(['student', message.id], (old) =>
            old ? { ...old, student: message.student } : old,
          );
          return;
        }

        if (message.type === 'student.deleted') {
          queryClient.setQueryData<Student[]>(['students'], (old) =>
            old ? old.filter((student) => student.id !== message.id) : old,
          );
          queryClient.removeQueries({ queryKey: ['student', message.id] });
        }
      };
    };

    connect();

    return () => {
      closedByEffect = true;
      if (retryTimer) {
        window.clearTimeout(retryTimer);
      }
      socket?.close();
    };
  }, [queryClient]);

  return status;
}

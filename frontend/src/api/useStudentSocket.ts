import { useEffect, useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { webSocketUrl } from './client';
import { fileManagerEntriesPrefix, fileManagerTrashKey } from './fileManagerKeys';
import { apiPaths } from './paths';
import type { StudentDetail, WsMessage } from './types';
import { useDirtyStore } from '../store/dirtyStore';

export function useStudentSocket(studentId: string | null) {
  const queryClient = useQueryClient();
  const hasDirtyConflict = useDirtyStore((state) => state.hasDirtyConflict);
  const [status, setStatus] = useState<'idle' | 'connected' | 'reconnecting'>('idle');

  useEffect(() => {
    if (!studentId) {
      setStatus('idle');
      return;
    }

    let socket: WebSocket | null = null;
    let closedByEffect = false;
    let retryTimer: number | undefined;

    const connect = () => {
      setStatus((current) => (current === 'idle' ? 'reconnecting' : current));
      socket = new WebSocket(webSocketUrl(apiPaths.studentSocket(studentId)));

      socket.onopen = () => setStatus('connected');
      socket.onclose = () => {
        if (!closedByEffect) {
          setStatus('reconnecting');
          retryTimer = window.setTimeout(connect, 1000);
        }
      };
      socket.onmessage = (event) => {
        const message = JSON.parse(event.data) as WsMessage;
        if (hasDirtyConflict(message)) {
          return;
        }

        if (message.type === 'file_manager.changed') {
          const context = { type: message.context_type, id: message.context_id };
          void queryClient.invalidateQueries({ queryKey: fileManagerEntriesPrefix(context) });
          void queryClient.invalidateQueries({ queryKey: fileManagerTrashKey(context) });
          return;
        }

        queryClient.setQueryData<StudentDetail>(['student', studentId], (old) => {
          if (!old) {
            return old;
          }
          if (message.type === 'student_info.updated') {
            return {
              ...old,
              info_fields: old.info_fields.map((field) =>
                field.id === message.info_field.id ? message.info_field : field,
              ),
            };
          }
          if (message.type === 'grade.created') {
            const exists = old.grades.some((grade) => grade.id === message.grade.id);
            return {
              ...old,
              grades: exists ? old.grades : [message.grade, ...old.grades],
            };
          }
          if (message.type === 'grade.updated') {
            return {
              ...old,
              grades: old.grades.map((grade) => (grade.id === message.grade.id ? message.grade : grade)),
            };
          }
          if (message.type === 'grade.deleted') {
            return {
              ...old,
              grades: old.grades.filter((grade) => grade.id !== message.id),
            };
          }
          return old;
        });
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
  }, [hasDirtyConflict, queryClient, studentId]);

  return status;
}

import { useEffect, useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { webSocketUrl } from './client';
import { fileManagerEntriesPrefix, fileManagerTrashKey } from './fileManagerKeys';
import { apiPaths } from './paths';
import {
  sortStudentInfoDefinitions,
  sortStudentInfoFields,
  studentInfoFieldFromDefinition,
} from './studentInfoFields';
import type { Student, StudentDetail, StudentInfoDefinition, WsMessage } from './types';
import { globalTemplatesContextType } from '../config/constants';
import { useDirtyStore } from '../store/dirtyStore';

function upsertStudent(students: Student[] = [], student: Student): Student[] {
  return [student, ...students.filter((item) => item.id !== student.id)];
}

export function useSchoolSocket() {
  const queryClient = useQueryClient();
  const hasDirtyConflict = useDirtyStore((state) => state.hasDirtyConflict);
  const [status, setStatus] = useState<'idle' | 'connected' | 'reconnecting'>('idle');

  useEffect(() => {
    let socket: WebSocket | null = null;
    let closedByEffect = false;
    let retryTimer: number | undefined;

    const connect = () => {
      setStatus((current) => (current === 'idle' ? 'reconnecting' : current));
      socket = new WebSocket(webSocketUrl(apiPaths.schoolSocket()));

      socket.onopen = () => setStatus('connected');
      socket.onclose = () => {
        if (!closedByEffect) {
          setStatus('reconnecting');
          retryTimer = window.setTimeout(connect, 1000);
        }
      };
      socket.onmessage = (event) => {
        const message = JSON.parse(event.data) as WsMessage;

        if (message.type === 'file_manager.changed' && message.context_type === globalTemplatesContextType) {
          const context = { type: message.context_type, id: message.context_id };
          void queryClient.invalidateQueries({ queryKey: fileManagerEntriesPrefix(context) });
          void queryClient.invalidateQueries({ queryKey: fileManagerTrashKey(context) });
          return;
        }

        if (message.resource === 'student' && (message.type === 'student.created' || message.type === 'student.updated')) {
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

        if (message.resource === 'student_info_definition') {
          if (hasDirtyConflict(message)) {
            return;
          }

          if (
            message.type === 'student_info_definition.created' ||
            message.type === 'student_info_definition.updated'
          ) {
            const definition = message.info_field_definition;
            queryClient.setQueryData<StudentInfoDefinition[]>(['student-info-definitions'], (old = []) =>
              sortStudentInfoDefinitions([definition, ...old.filter((field) => field.id !== definition.id)]),
            );
            queryClient.setQueriesData<StudentDetail>({ queryKey: ['student'] }, (old) => {
              if (!old) {
                return old;
              }
              const existing = old.info_fields.find((field) => field.id === definition.id);
              const nextField = studentInfoFieldFromDefinition(definition, existing);
              return {
                ...old,
                info_fields: sortStudentInfoFields([
                  nextField,
                  ...old.info_fields.filter((field) => field.id !== definition.id),
                ]),
              };
            });
            return;
          }

          if (message.type === 'student_info_definition.deleted') {
            queryClient.setQueryData<StudentInfoDefinition[]>(['student-info-definitions'], (old) =>
              old ? old.filter((field) => field.id !== message.id) : old,
            );
            queryClient.setQueriesData<StudentDetail>({ queryKey: ['student'] }, (old) =>
              old
                ? {
                    ...old,
                    info_fields: old.info_fields.filter((field) => field.id !== message.id),
                  }
                : old,
            );
          }
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
  }, [hasDirtyConflict, queryClient]);

  return status;
}

import { useEffect, useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { WS_BASE_URL } from './client';
import type { Student, StudentDetail, StudentInfoDefinition, StudentInfoField, WsMessage } from './types';
import { useDirtyStore } from '../store/dirtyStore';

function upsertStudent(students: Student[] = [], student: Student): Student[] {
  return [student, ...students.filter((item) => item.id !== student.id)];
}

function sortInfoDefinitions(definitions: StudentInfoDefinition[]): StudentInfoDefinition[] {
  return [...definitions].sort((left, right) => {
    const displayCompare = left.display_name.localeCompare(right.display_name, undefined, {
      sensitivity: 'base',
    });
    return displayCompare === 0 ? left.name.localeCompare(right.name) : displayCompare;
  });
}

function isValueCompatible(valueType: StudentInfoDefinition['value_type'], value: string): boolean {
  if (value === '') {
    return true;
  }
  if (valueType === 'INTEGER') {
    return /^[+-]?\d+$/.test(value);
  }
  if (valueType === 'DATE') {
    return /^\d{4}-\d{2}-\d{2}$/.test(value);
  }
  return true;
}

function fieldFromDefinition(
  definition: StudentInfoDefinition,
  existing?: StudentInfoField,
): StudentInfoField {
  const value = existing && isValueCompatible(definition.value_type, existing.value) ? existing.value : '';
  return {
    id: definition.id,
    name: definition.name,
    display_name: definition.display_name,
    value_type: definition.value_type,
    value,
    updated_at: Math.max(definition.updated_at, existing?.updated_at ?? definition.updated_at),
  };
}

function sortStudentInfoFields(fields: StudentInfoField[]): StudentInfoField[] {
  return [...fields].sort((left, right) => {
    const displayCompare = left.display_name.localeCompare(right.display_name, undefined, {
      sensitivity: 'base',
    });
    return displayCompare === 0 ? left.name.localeCompare(right.name) : displayCompare;
  });
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
              sortInfoDefinitions([definition, ...old.filter((field) => field.id !== definition.id)]),
            );
            queryClient.setQueriesData<StudentDetail>({ queryKey: ['student'] }, (old) => {
              if (!old) {
                return old;
              }
              const existing = old.info_fields.find((field) => field.id === definition.id);
              const nextField = fieldFromDefinition(definition, existing);
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

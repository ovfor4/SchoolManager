import { create } from 'zustand';
import type { WsMessage } from '../api/types';

type DirtyState = {
  dirtyFields: Record<string, true>;
  markDirty: (fieldId: string) => void;
  clearDirty: (fieldId: string) => void;
  clearStudentInfoDefinitionDirty: (infoFieldId: string) => void;
  clearStudentInfoValueDirty: (studentId: string, infoFieldId: string) => void;
  clearGradeDirty: (gradeId: string) => void;
  hasDirtyConflict: (message: WsMessage) => boolean;
};

export const useDirtyStore = create<DirtyState>((set, get) => ({
  dirtyFields: {},
  markDirty: (fieldId) =>
    set((state) => ({
      dirtyFields: { ...state.dirtyFields, [fieldId]: true },
    })),
  clearDirty: (fieldId) =>
    set((state) => {
      const next = { ...state.dirtyFields };
      delete next[fieldId];
      return { dirtyFields: next };
    }),
  clearStudentInfoDefinitionDirty: (infoFieldId) =>
    set((state) => {
      const next = { ...state.dirtyFields };
      for (const key of Object.keys(next)) {
        if (key.startsWith(`student_info_definition:${infoFieldId}:`)) {
          delete next[key];
        }
      }
      return { dirtyFields: next };
    }),
  clearStudentInfoValueDirty: (studentId, infoFieldId) =>
    set((state) => {
      const next = { ...state.dirtyFields };
      for (const key of Object.keys(next)) {
        if (key.startsWith(`student_info_value:${studentId}:${infoFieldId}:`)) {
          delete next[key];
        }
      }
      return { dirtyFields: next };
    }),
  clearGradeDirty: (gradeId) =>
    set((state) => {
      const next = { ...state.dirtyFields };
      for (const key of Object.keys(next)) {
        if (key.startsWith(`grade:${gradeId}:`)) {
          delete next[key];
        }
      }
      return { dirtyFields: next };
    }),
  hasDirtyConflict: (message) => {
    const dirty = get().dirtyFields;
    if (message.resource === 'grade' && (message.type === 'grade.updated' || message.type === 'grade.created')) {
      if (dirty[message.field_id]) {
        return true;
      }
      return Boolean(message.changed_fields?.some((field) => dirty[`grade:${message.id}:${field}`]));
    }
    if (
      message.resource === 'student_info' &&
      message.type === 'student_info.updated'
    ) {
      if (dirty[message.field_id]) {
        return true;
      }
      return Boolean(
        message.changed_fields?.some(
          (field) => dirty[`student_info_value:${message.student_id}:${message.id}:${field}`],
        ),
      );
    }
    if (
      message.resource === 'student_info_definition' &&
      (message.type === 'student_info_definition.updated' ||
        message.type === 'student_info_definition.created')
    ) {
      if (dirty[message.field_id]) {
        return true;
      }
      return Boolean(
        message.changed_fields?.some((field) => dirty[`student_info_definition:${message.id}:${field}`]),
      );
    }
    return Boolean(dirty[message.field_id]);
  },
}));

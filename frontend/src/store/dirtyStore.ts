import { create } from 'zustand';
import type { WsMessage } from '../api/types';

type DirtyState = {
  dirtyFields: Record<string, true>;
  markDirty: (fieldId: string) => void;
  clearDirty: (fieldId: string) => void;
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
    return Boolean(dirty[message.field_id]);
  },
}));

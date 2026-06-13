import type { FileContext } from './types';

export const fileManagerEntriesKey = (context: FileContext, parentId: string | null) => [
  'file-manager',
  'entries',
  context.type,
  context.id,
  parentId ?? 'root',
];

export const fileManagerEntriesPrefix = (context: FileContext) => [
  'file-manager',
  'entries',
  context.type,
  context.id,
];

export const fileManagerTrashKey = (context: FileContext) => [
  'file-manager',
  'trash',
  context.type,
  context.id,
];

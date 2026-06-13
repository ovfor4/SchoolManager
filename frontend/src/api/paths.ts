import {
  apiRootPath,
  webSocketSchoolScope,
  webSocketScopeParameter,
  webSocketStudentIdParameter,
  webSocketStudentsPath,
} from '../config/constants';
import type { FileContext } from './types';

const segment = (value: string) => encodeURIComponent(value);

const studentPath = (studentId: string) => `${apiRootPath}/students/${segment(studentId)}`;
const fileContextPath = (context: FileContext) =>
  `${apiRootPath}/file-manager/contexts/${segment(context.type)}/${segment(context.id)}`;
const query = (params: Record<string, string | null | undefined>) => {
  const search = new URLSearchParams();
  for (const [key, value] of Object.entries(params)) {
    if (value) {
      search.set(key, value);
    }
  }
  const text = search.toString();
  return text ? `?${text}` : '';
};

export const apiPaths = {
  students: () => `${apiRootPath}/students`,
  student: (studentId: string) => studentPath(studentId),
  studentInfoDefinitions: () => `${apiRootPath}/student-info-fields`,
  studentInfoDefinition: (fieldId: string) => `${apiRootPath}/student-info-fields/${segment(fieldId)}`,
  studentInfoValue: (studentId: string, fieldId: string) =>
    `${studentPath(studentId)}/info-fields/${segment(fieldId)}`,
  grades: (studentId: string) => `${studentPath(studentId)}/grades`,
  grade: (studentId: string, gradeId: string) => `${studentPath(studentId)}/grades/${segment(gradeId)}`,
  fileEntries: (context: FileContext, parentId?: string | null) =>
    `${fileContextPath(context)}/entries${query({ parent_id: parentId })}`,
  fileUpload: (context: FileContext, parentId?: string | null) =>
    `${fileContextPath(context)}/files${query({ parent_id: parentId })}`,
  fileFolders: (context: FileContext) => `${fileContextPath(context)}/folders`,
  fileEntry: (context: FileContext, entryId: string) => `${fileContextPath(context)}/entries/${segment(entryId)}`,
  fileEntryDownload: (context: FileContext, entryId: string) =>
    `${fileContextPath(context)}/entries/${segment(entryId)}/download`,
  fileTrash: (context: FileContext) => `${fileContextPath(context)}/trash`,
  fileTrashRestore: (context: FileContext, trashEntryId: string) =>
    `${fileContextPath(context)}/trash/${segment(trashEntryId)}/restore`,
  fileTrashEntry: (context: FileContext, trashEntryId: string) =>
    `${fileContextPath(context)}/trash/${segment(trashEntryId)}`,
  schoolSocket: () =>
    `${webSocketStudentsPath}?${webSocketScopeParameter}=${encodeURIComponent(webSocketSchoolScope)}`,
  studentSocket: (studentId: string) =>
    `${webSocketStudentsPath}?${webSocketStudentIdParameter}=${segment(studentId)}`,
};

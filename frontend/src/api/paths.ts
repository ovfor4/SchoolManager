import {
  apiRootPath,
  webSocketSchoolScope,
  webSocketScopeParameter,
  webSocketStudentIdParameter,
  webSocketStudentsPath,
} from '../config/constants';

const segment = (value: string) => encodeURIComponent(value);

const studentPath = (studentId: string) => `${apiRootPath}/students/${segment(studentId)}`;

export const apiPaths = {
  students: () => `${apiRootPath}/students`,
  student: (studentId: string) => studentPath(studentId),
  studentInfoDefinitions: () => `${apiRootPath}/student-info-fields`,
  studentInfoDefinition: (fieldId: string) => `${apiRootPath}/student-info-fields/${segment(fieldId)}`,
  studentInfoValue: (studentId: string, fieldId: string) =>
    `${studentPath(studentId)}/info-fields/${segment(fieldId)}`,
  grades: (studentId: string) => `${studentPath(studentId)}/grades`,
  grade: (studentId: string, gradeId: string) => `${studentPath(studentId)}/grades/${segment(gradeId)}`,
  files: (studentId: string) => `${studentPath(studentId)}/files`,
  fileDownload: (studentId: string, fileId: string) =>
    `${studentPath(studentId)}/files/${segment(fileId)}/download`,
  schoolSocket: () =>
    `${webSocketStudentsPath}?${webSocketScopeParameter}=${encodeURIComponent(webSocketSchoolScope)}`,
  studentSocket: (studentId: string) =>
    `${webSocketStudentsPath}?${webSocketStudentIdParameter}=${segment(studentId)}`,
};

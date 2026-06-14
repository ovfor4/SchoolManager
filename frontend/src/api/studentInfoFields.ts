import type { StudentInfoDefinition, StudentInfoField, StudentInfoValueType } from './types';

export function isStudentInfoValueCompatible(valueType: StudentInfoValueType, value: string): boolean {
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

export function studentInfoFieldFromDefinition(
  definition: StudentInfoDefinition,
  existing?: StudentInfoField,
): StudentInfoField {
  const value =
    existing && isStudentInfoValueCompatible(definition.value_type, existing.value) ? existing.value : '';
  return {
    id: definition.id,
    name: definition.name,
    display_name: definition.display_name,
    value_type: definition.value_type,
    value,
    updated_at: Math.max(definition.updated_at, existing?.updated_at ?? definition.updated_at),
  };
}

export function sortStudentInfoDefinitions(
  definitions: StudentInfoDefinition[],
): StudentInfoDefinition[] {
  return [...definitions].sort((left, right) => {
    const displayCompare = left.display_name.localeCompare(right.display_name, undefined, {
      sensitivity: 'base',
    });
    return displayCompare === 0 ? left.name.localeCompare(right.name) : displayCompare;
  });
}

export function sortStudentInfoFields(fields: StudentInfoField[]): StudentInfoField[] {
  return [...fields].sort((left, right) => {
    const displayCompare = left.display_name.localeCompare(right.display_name, undefined, {
      sensitivity: 'base',
    });
    return displayCompare === 0 ? left.name.localeCompare(right.name) : displayCompare;
  });
}

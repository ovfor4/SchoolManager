import { useEffect, useMemo, useState } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { patchStudentInfoValue } from '../api/client';
import type { StudentDetail, StudentInfoField } from '../api/types';
import { useDirtyStore } from '../store/dirtyStore';

type StudentInfoPanelProps = {
  studentId: string;
  infoFields: StudentInfoField[];
};

function inputTypeFor(field: StudentInfoField): 'date' | 'number' | 'text' {
  if (field.value_type === 'DATE') {
    return 'date';
  }
  if (field.value_type === 'INTEGER') {
    return 'number';
  }
  return 'text';
}

export function StudentInfoPanel({ studentId, infoFields }: StudentInfoPanelProps) {
  const queryClient = useQueryClient();
  const dirtyFields = useDirtyStore((state) => state.dirtyFields);
  const markDirty = useDirtyStore((state) => state.markDirty);
  const clearDirty = useDirtyStore((state) => state.clearDirty);
  const [drafts, setDrafts] = useState<Record<string, string>>({});

  const infoFieldById = useMemo(
    () => new Map(infoFields.map((field) => [field.id, field])),
    [infoFields],
  );

  useEffect(() => {
    setDrafts((current) => {
      const next: Record<string, string> = {};
      for (const incoming of infoFields) {
        const dirtyKey = `student_info_value:${studentId}:${incoming.id}:value`;
        next[incoming.id] = dirtyFields[dirtyKey] ? (current[incoming.id] ?? incoming.value) : incoming.value;
      }
      return next;
    });
  }, [dirtyFields, infoFields, studentId]);

  const patchMutation = useMutation({
    mutationKey: ['patch-student-info-value', studentId],
    mutationFn: ({ fieldId, value }: { fieldId: string; value: string }) =>
      patchStudentInfoValue(studentId, fieldId, { value }),
    onSuccess: (infoField, variables) => {
      clearDirty(`student_info_value:${studentId}:${variables.fieldId}:value`);
      queryClient.setQueryData<StudentDetail>(['student', studentId], (old) =>
        old
          ? {
              ...old,
              info_fields: old.info_fields.map((field) =>
                field.id === infoField.id ? infoField : field,
              ),
            }
          : old,
      );
    },
  });

  const updateDraft = (fieldId: string, value: string) => {
    const original = infoFieldById.get(fieldId);
    if (!original) {
      return;
    }
    markDirty(`student_info_value:${studentId}:${fieldId}:value`);
    setDrafts((current) => ({
      ...current,
      [fieldId]: value,
    }));
  };

  const commitValue = (fieldId: string) => {
    const original = infoFieldById.get(fieldId);
    if (!original) {
      return;
    }

    const value = drafts[fieldId] ?? original.value;
    const dirtyKey = `student_info_value:${studentId}:${fieldId}:value`;
    if (value === original.value) {
      clearDirty(dirtyKey);
      return;
    }

    patchMutation.mutate({ fieldId, value });
  };

  return (
    <section className="workspaceSection">
      <div className="sectionHeader">
        <div>
          <h2>Student Info</h2>
          <span>{infoFields.length} fields</span>
        </div>
      </div>

      {infoFields.length === 0 ? (
        <div className="emptyCell">No student info fields</div>
      ) : (
        <div className="infoFormFrame">
          {infoFields.map((field) => {
            const dirtyKey = `student_info_value:${studentId}:${field.id}:value`;
            const value = drafts[field.id] ?? field.value;
            return (
              <label key={field.id} className="infoFieldRow">
                <span>
                  <strong>{field.display_name}</strong>
                  <small>{field.name}</small>
                </span>
                <input
                  type={inputTypeFor(field)}
                  inputMode={field.value_type === 'INTEGER' ? 'numeric' : undefined}
                  step={field.value_type === 'INTEGER' ? 1 : undefined}
                  className={[
                    dirtyFields[dirtyKey] ? 'dirtyInput' : '',
                    field.value_type === 'DATE' ? 'dateInput' : '',
                  ]
                    .filter(Boolean)
                    .join(' ')}
                  value={value}
                  onChange={(event) => updateDraft(field.id, event.target.value)}
                  onBlur={() => commitValue(field.id)}
                  aria-label={`Value for ${field.display_name || field.name}`}
                />
              </label>
            );
          })}
        </div>
      )}

      {patchMutation.error ? <div className="inlineError">{patchMutation.error.message}</div> : null}
    </section>
  );
}

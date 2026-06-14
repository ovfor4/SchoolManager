import { useEffect, useMemo, useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { Plus, Trash2 } from 'lucide-react';
import {
  createStudentInfoDefinition,
  deleteStudentInfoDefinition,
  listStudentInfoDefinitions,
  patchStudentInfoDefinition,
} from '../api/client';
import {
  sortStudentInfoDefinitions,
  sortStudentInfoFields,
  studentInfoFieldFromDefinition,
} from '../api/studentInfoFields';
import type {
  StudentDetail,
  StudentInfoDefinition,
  StudentInfoDefinitionPatch,
  StudentInfoValueType,
} from '../api/types';
import { useDirtyStore } from '../store/dirtyStore';

const definitionFields = ['name', 'display_name', 'value_type'] as const;
const valueTypes: StudentInfoValueType[] = ['STRING', 'INTEGER', 'DATE'];
type DefinitionField = (typeof definitionFields)[number];

function nextFieldName(infoFields: StudentInfoDefinition[]): string {
  const existingNames = new Set(infoFields.map((field) => field.name));
  let index = infoFields.length + 1;
  while (existingNames.has(`field_${index}`)) {
    index += 1;
  }
  return `field_${index}`;
}

function normalizeCommitValue(field: DefinitionField, value: string, draft: StudentInfoDefinition): string {
  if (field === 'name') {
    return value.trim();
  }
  if (field === 'display_name') {
    return value.trim() || draft.name.trim() || 'Field';
  }
  return value;
}

export function StudentInfoDefinitionsPage() {
  const queryClient = useQueryClient();
  const dirtyFields = useDirtyStore((state) => state.dirtyFields);
  const markDirty = useDirtyStore((state) => state.markDirty);
  const clearDirty = useDirtyStore((state) => state.clearDirty);
  const clearStudentInfoDefinitionDirty = useDirtyStore(
    (state) => state.clearStudentInfoDefinitionDirty,
  );
  const [drafts, setDrafts] = useState<Record<string, StudentInfoDefinition>>({});

  const definitionsQuery = useQuery({
    queryKey: ['student-info-definitions'],
    queryFn: listStudentInfoDefinitions,
  });

  const definitions = definitionsQuery.data ?? [];
  const definitionById = useMemo(
    () => new Map(definitions.map((field) => [field.id, field])),
    [definitions],
  );

  useEffect(() => {
    setDrafts((current) => {
      const next: Record<string, StudentInfoDefinition> = {};
      for (const incoming of definitions) {
        const existing = current[incoming.id];
        if (!existing) {
          next[incoming.id] = incoming;
          continue;
        }
        const merged = { ...incoming };
        for (const field of definitionFields) {
          if (dirtyFields[`student_info_definition:${incoming.id}:${field}`]) {
            merged[field] = existing[field] as never;
          }
        }
        next[incoming.id] = merged;
      }
      return next;
    });
  }, [definitions, dirtyFields]);

  const applyDefinitionToStudentDetails = (definition: StudentInfoDefinition) => {
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
  };

  const createMutation = useMutation({
    mutationKey: ['create-student-info-definition'],
    mutationFn: () => {
      const name = nextFieldName(definitions);
      return createStudentInfoDefinition({
        name,
        display_name: 'New field',
        value_type: 'STRING',
      });
    },
    onSuccess: (definition) => {
      queryClient.setQueryData<StudentInfoDefinition[]>(['student-info-definitions'], (old = []) => [
        ...sortStudentInfoDefinitions([definition, ...old.filter((field) => field.id !== definition.id)]),
      ]);
      applyDefinitionToStudentDetails(definition);
    },
  });

  const patchMutation = useMutation({
    mutationKey: ['patch-student-info-definition'],
    mutationFn: ({
      fieldId,
      patch,
    }: {
      fieldId: string;
      fields: DefinitionField[];
      patch: StudentInfoDefinitionPatch;
    }) => patchStudentInfoDefinition(fieldId, patch),
    onSuccess: (definition, variables) => {
      for (const field of variables.fields) {
        clearDirty(`student_info_definition:${variables.fieldId}:${field}`);
      }
      queryClient.setQueryData<StudentInfoDefinition[]>(['student-info-definitions'], (old) =>
        old
          ? sortStudentInfoDefinitions(
              old.map((field) => (field.id === definition.id ? definition : field)),
            )
          : old,
      );
      applyDefinitionToStudentDetails(definition);
    },
  });

  const deleteMutation = useMutation({
    mutationKey: ['delete-student-info-definition'],
    mutationFn: (fieldId: string) => deleteStudentInfoDefinition(fieldId).then(() => fieldId),
    onSuccess: (fieldId) => {
      clearStudentInfoDefinitionDirty(fieldId);
      queryClient.setQueryData<StudentInfoDefinition[]>(['student-info-definitions'], (old) =>
        old ? old.filter((field) => field.id !== fieldId) : old,
      );
      queryClient.setQueriesData<StudentDetail>({ queryKey: ['student'] }, (old) =>
        old
          ? {
              ...old,
              info_fields: old.info_fields.filter((field) => field.id !== fieldId),
            }
          : old,
      );
    },
  });

  const updateDraft = (fieldId: string, field: DefinitionField, value: string) => {
    const original = definitionById.get(fieldId);
    if (!original) {
      return;
    }
    markDirty(`student_info_definition:${fieldId}:${field}`);
    setDrafts((current) => ({
      ...current,
      [fieldId]: {
        ...(current[fieldId] ?? original),
        [field]: value,
      },
    }));
  };

  const commitFields = (
    fieldId: string,
    fields: DefinitionField[],
    draftOverride: Partial<StudentInfoDefinition> = {},
  ) => {
    const original = definitionById.get(fieldId);
    if (!original) {
      return;
    }

    const draft = { ...(drafts[fieldId] ?? original), ...draftOverride };
    const patch: Record<string, string> = {};
    const changedFields: DefinitionField[] = [];

    for (const field of fields) {
      const nextValue = normalizeCommitValue(field, String(draft[field]), draft);
      if (nextValue === String(original[field])) {
        clearDirty(`student_info_definition:${fieldId}:${field}`);
        continue;
      }
      patch[field] = nextValue;
      changedFields.push(field);
    }

    if (changedFields.length === 0) {
      return;
    }

    patchMutation.mutate({
      fieldId,
      fields: changedFields,
      patch: patch as StudentInfoDefinitionPatch,
    });
  };

  const changeValueType = (definition: StudentInfoDefinition, valueType: StudentInfoValueType) => {
    markDirty(`student_info_definition:${definition.id}:value_type`);
    setDrafts((current) => ({
      ...current,
      [definition.id]: {
        ...(current[definition.id] ?? definition),
        value_type: valueType,
      },
    }));
    commitFields(definition.id, ['value_type'], { value_type: valueType });
  };

  const removeDefinition = (definition: StudentInfoDefinition) => {
    if (window.confirm(`Delete ${definition.display_name}?`)) {
      deleteMutation.mutate(definition.id);
    }
  };

  return (
    <section className="workspaceSection">
      <div className="sectionHeader">
        <div>
          <h2>Student Info</h2>
          <span>{definitions.length} configured</span>
        </div>
        <button
          className="iconTextButton"
          onClick={() => createMutation.mutate()}
          disabled={createMutation.isPending}
        >
          <Plus size={18} />
          Add
        </button>
      </div>

      <div className="tableFrame">
        <table className="infoTable">
          <thead>
            <tr>
              <th>Unique name</th>
              <th>Display name</th>
              <th>Type</th>
              <th aria-label="Actions" />
            </tr>
          </thead>
          <tbody>
            {definitionsQuery.isLoading ? (
              <tr>
                <td colSpan={4} className="emptyCell">
                  Loading fields...
                </td>
              </tr>
            ) : definitions.length === 0 ? (
              <tr>
                <td colSpan={4} className="emptyCell">
                  No student info fields
                </td>
              </tr>
            ) : (
              definitions.map((definition) => {
                const draft = drafts[definition.id] ?? definition;
                return (
                  <tr key={definition.id}>
                    <td>
                      <input
                        className={
                          dirtyFields[`student_info_definition:${definition.id}:name`] ? 'dirtyInput' : ''
                        }
                        value={draft.name}
                        onChange={(event) => updateDraft(definition.id, 'name', event.target.value)}
                        onBlur={() => commitFields(definition.id, ['name'])}
                        aria-label={`Unique name for ${definition.display_name || definition.id}`}
                      />
                    </td>
                    <td>
                      <input
                        className={
                          dirtyFields[`student_info_definition:${definition.id}:display_name`]
                            ? 'dirtyInput'
                            : ''
                        }
                        value={draft.display_name}
                        onChange={(event) =>
                          updateDraft(definition.id, 'display_name', event.target.value)
                        }
                        onBlur={() => commitFields(definition.id, ['display_name'])}
                        aria-label={`Display name for ${definition.name || definition.id}`}
                      />
                    </td>
                    <td>
                      <select
                        className={
                          dirtyFields[`student_info_definition:${definition.id}:value_type`]
                            ? 'dirtyInput'
                            : ''
                        }
                        value={draft.value_type}
                        onChange={(event) =>
                          changeValueType(definition, event.target.value as StudentInfoValueType)
                        }
                        aria-label={`Type for ${definition.display_name || definition.name}`}
                      >
                        {valueTypes.map((valueType) => (
                          <option key={valueType} value={valueType}>
                            {valueType}
                          </option>
                        ))}
                      </select>
                    </td>
                    <td className="rowAction">
                      <button
                        className="iconButton danger"
                        onClick={() => removeDefinition(definition)}
                        aria-label="Delete student info field"
                      >
                        <Trash2 size={17} />
                      </button>
                    </td>
                  </tr>
                );
              })
            )}
          </tbody>
        </table>
      </div>
      {definitionsQuery.error ? <div className="inlineError">{definitionsQuery.error.message}</div> : null}
      {createMutation.error ? <div className="inlineError">{createMutation.error.message}</div> : null}
      {patchMutation.error ? <div className="inlineError">{patchMutation.error.message}</div> : null}
      {deleteMutation.error ? <div className="inlineError">{deleteMutation.error.message}</div> : null}
    </section>
  );
}

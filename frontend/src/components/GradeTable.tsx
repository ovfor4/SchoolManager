import { useEffect, useMemo, useState } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { Plus, Trash2 } from 'lucide-react';
import { createGrade, deleteGrade, patchGrade } from '../api/client';
import type { Grade, GradePatch, StudentDetail } from '../api/types';
import { useDirtyStore } from '../store/dirtyStore';

const gradeFields = ['title', 'score', 'max_score', 'occurred_on', 'notes'] as const;
type GradeField = (typeof gradeFields)[number];

type GradeTableProps = {
  studentId: string;
  grades: Grade[];
};

export function GradeTable({ studentId, grades }: GradeTableProps) {
  const queryClient = useQueryClient();
  const dirtyFields = useDirtyStore((state) => state.dirtyFields);
  const markDirty = useDirtyStore((state) => state.markDirty);
  const clearDirty = useDirtyStore((state) => state.clearDirty);
  const clearGradeDirty = useDirtyStore((state) => state.clearGradeDirty);
  const [drafts, setDrafts] = useState<Record<string, Grade>>({});

  const gradeById = useMemo(() => new Map(grades.map((grade) => [grade.id, grade])), [grades]);

  useEffect(() => {
    setDrafts((current) => {
      const next: Record<string, Grade> = {};
      for (const incoming of grades) {
        const existing = current[incoming.id];
        if (!existing) {
          next[incoming.id] = incoming;
          continue;
        }
        const merged = { ...incoming };
        for (const field of gradeFields) {
          if (dirtyFields[`grade:${incoming.id}:${field}`]) {
            merged[field] = existing[field];
          }
        }
        next[incoming.id] = merged;
      }
      return next;
    });
  }, [dirtyFields, grades]);

  const createMutation = useMutation({
    mutationKey: ['create-grade', studentId],
    mutationFn: () => createGrade(studentId),
    onSuccess: (grade) => {
      queryClient.setQueryData<StudentDetail>(['student', studentId], (old) => {
        if (!old) {
          return old;
        }
        const existingGrades = old.grades.filter((item) => item.id !== grade.id);
        return { ...old, grades: [grade, ...existingGrades] };
      });
    },
  });

  const patchMutation = useMutation({
    mutationKey: ['patch-grade', studentId],
    mutationFn: ({ gradeId, patch }: { gradeId: string; field: GradeField; patch: GradePatch }) =>
      patchGrade(studentId, gradeId, patch),
    onSuccess: (grade, variables) => {
      clearDirty(`grade:${variables.gradeId}:${variables.field}`);
      queryClient.setQueryData<StudentDetail>(['student', studentId], (old) =>
        old ? { ...old, grades: old.grades.map((item) => (item.id === grade.id ? grade : item)) } : old,
      );
    },
  });

  const deleteMutation = useMutation({
    mutationKey: ['delete-grade', studentId],
    mutationFn: (gradeId: string) => deleteGrade(studentId, gradeId).then(() => gradeId),
    onSuccess: (gradeId) => {
      clearGradeDirty(gradeId);
      queryClient.setQueryData<StudentDetail>(['student', studentId], (old) =>
        old ? { ...old, grades: old.grades.filter((grade) => grade.id !== gradeId) } : old,
      );
    },
  });

  const updateDraft = (gradeId: string, field: GradeField, value: string) => {
    markDirty(`grade:${gradeId}:${field}`);
    setDrafts((current) => ({
      ...current,
      [gradeId]: {
        ...current[gradeId],
        [field]: value,
      },
    }));
  };

  const commitField = (gradeId: string, field: GradeField) => {
    const draft = drafts[gradeId];
    const original = gradeById.get(gradeId);
    if (!draft || !original) {
      return;
    }
    if (draft[field] === original[field]) {
      clearDirty(`grade:${gradeId}:${field}`);
      return;
    }
    patchMutation.mutate({
      gradeId,
      field,
      patch: { [field]: draft[field] },
    });
  };

  return (
    <section className="workspaceSection">
      <div className="sectionHeader">
        <div>
          <h2>Grades</h2>
          <span>{grades.length} records</span>
        </div>
        <button className="iconTextButton" onClick={() => createMutation.mutate()} disabled={createMutation.isPending}>
          <Plus size={18} />
          Add
        </button>
      </div>

      <div className="tableFrame">
        <table className="gradeTable">
          <thead>
            <tr>
              <th>Name</th>
              <th>Score</th>
              <th>Max</th>
              <th>Date</th>
              <th>Notes</th>
              <th aria-label="Actions" />
            </tr>
          </thead>
          <tbody>
            {grades.length === 0 ? (
              <tr>
                <td colSpan={6} className="emptyCell">
                  No grade records
                </td>
              </tr>
            ) : (
              grades.map((grade) => {
                const draft = drafts[grade.id] ?? grade;
                return (
                  <tr key={grade.id}>
                    {gradeFields.map((field) => {
                      const inputClassName = [
                        dirtyFields[`grade:${grade.id}:${field}`] ? 'dirtyInput' : '',
                        field === 'occurred_on' ? 'dateInput' : '',
                      ]
                        .filter(Boolean)
                        .join(' ');
                      return (
                        <td key={field}>
                          <input
                            type={field === 'occurred_on' ? 'date' : 'text'}
                            className={inputClassName}
                            value={draft[field]}
                            onChange={(event) => updateDraft(grade.id, field, event.target.value)}
                            onBlur={() => commitField(grade.id, field)}
                            aria-label={`${field} for ${grade.title || grade.id}`}
                          />
                        </td>
                      );
                    })}
                    <td className="rowAction">
                      <button
                        className="iconButton danger"
                        onClick={() => deleteMutation.mutate(grade.id)}
                        aria-label="Delete grade"
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
      {patchMutation.error ? <div className="inlineError">{patchMutation.error.message}</div> : null}
    </section>
  );
}

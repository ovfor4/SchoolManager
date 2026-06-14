import { FormEvent, useEffect, useState } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { ChevronDown, ChevronRight, Files, ListChecks, Plus, Trash2, UserRound } from 'lucide-react';
import { createStudent, deleteStudent, patchStudent } from '../api/client';
import type { Student, StudentDetail } from '../api/types';

export type AppView = 'student' | 'student-info-settings' | 'template-library-settings' | 'batch';

type StudentSidebarProps = {
  students: Student[];
  selectedStudentId: string | null;
  activeView: AppView;
  loading: boolean;
  onSelect: (studentId: string | null) => void;
  onOpenStudentInfoSettings: () => void;
  onOpenTemplateLibrarySettings: () => void;
  onOpenBatch: () => void;
};

export function StudentSidebar({
  students,
  selectedStudentId,
  activeView,
  loading,
  onSelect,
  onOpenStudentInfoSettings,
  onOpenTemplateLibrarySettings,
  onOpenBatch,
}: StudentSidebarProps) {
  const [displayName, setDisplayName] = useState('');
  const [studentDrafts, setStudentDrafts] = useState<Record<string, string>>({});
  const [dirtyStudentNames, setDirtyStudentNames] = useState<Record<string, boolean>>({});
  const [settingsExpanded, setSettingsExpanded] = useState(false);
  const [templateSettingsExpanded, setTemplateSettingsExpanded] = useState(false);
  const queryClient = useQueryClient();
  const settingsActive =
    activeView === 'student-info-settings' || activeView === 'template-library-settings';

  useEffect(() => {
    setStudentDrafts((current) => {
      const next: Record<string, string> = {};
      for (const student of students) {
        next[student.id] = dirtyStudentNames[student.id]
          ? (current[student.id] ?? student.display_name)
          : student.display_name;
      }
      return next;
    });
  }, [dirtyStudentNames, students]);

  useEffect(() => {
    if (settingsActive) {
      setSettingsExpanded(true);
    }
    if (activeView === 'template-library-settings') {
      setTemplateSettingsExpanded(true);
    }
  }, [activeView, settingsActive]);

  const createMutation = useMutation({
    mutationKey: ['create-student'],
    mutationFn: createStudent,
    onSuccess: (student) => {
      queryClient.setQueryData<Student[]>(['students'], (old = []) => [
        student,
        ...old.filter((item) => item.id !== student.id),
      ]);
      setDisplayName('');
      onSelect(student.id);
    },
  });

  const patchMutation = useMutation({
    mutationKey: ['patch-student'],
    mutationFn: ({ studentId, nextDisplayName }: { studentId: string; nextDisplayName: string }) =>
      patchStudent(studentId, { display_name: nextDisplayName }),
    onSuccess: (student) => {
      setDirtyStudentNames((current) => ({ ...current, [student.id]: false }));
      setStudentDrafts((current) => ({ ...current, [student.id]: student.display_name }));
      queryClient.setQueryData<Student[]>(['students'], (old) =>
        old ? [student, ...old.filter((item) => item.id !== student.id)] : old,
      );
      queryClient.setQueryData<StudentDetail>(['student', student.id], (old) =>
        old ? { ...old, student } : old,
      );
    },
  });

  const deleteMutation = useMutation({
    mutationKey: ['delete-student'],
    mutationFn: deleteStudent,
    onSuccess: (_, studentId) => {
      const remainingStudents = students.filter((student) => student.id !== studentId);
      setDirtyStudentNames((current) => {
        const next = { ...current };
        delete next[studentId];
        return next;
      });
      setStudentDrafts((current) => {
        const next = { ...current };
        delete next[studentId];
        return next;
      });
      queryClient.setQueryData<Student[]>(['students'], (old) =>
        old ? old.filter((student) => student.id !== studentId) : old,
      );
      queryClient.removeQueries({ queryKey: ['student', studentId] });
      if (selectedStudentId === studentId) {
        onSelect(remainingStudents[0]?.id ?? null);
      }
    },
  });

  const submit = (event: FormEvent) => {
    event.preventDefault();
    createMutation.mutate(displayName.trim() || 'Unnamed student');
  };

  const updateStudentDraft = (studentId: string, value: string) => {
    setDirtyStudentNames((current) => ({ ...current, [studentId]: true }));
    setStudentDrafts((current) => ({ ...current, [studentId]: value }));
  };

  const commitStudentName = (studentId: string) => {
    const student = students.find((item) => item.id === studentId);
    if (!student) {
      return;
    }
    const nextDisplayName = (studentDrafts[studentId] ?? student.display_name).trim() || 'Unnamed student';
    if (nextDisplayName === student.display_name) {
      setDirtyStudentNames((current) => ({ ...current, [studentId]: false }));
      setStudentDrafts((current) => ({ ...current, [studentId]: student.display_name }));
      return;
    }
    setStudentDrafts((current) => ({ ...current, [studentId]: nextDisplayName }));
    patchMutation.mutate({ studentId, nextDisplayName });
  };

  const removeStudent = (student: Student) => {
    if (window.confirm(`Delete ${student.display_name}?`)) {
      deleteMutation.mutate(student.id);
    }
  };

  return (
    <aside className="studentRail">
      <div className="brandBlock">
        <span className="brandMark">SM</span>
        <div>
          <strong>SchoolManager</strong>
          <span>Grades</span>
        </div>
      </div>

      <button
        type="button"
        className={`sidebarNavButton ${settingsActive ? 'active' : ''}`}
        onClick={() => {
          setSettingsExpanded((current) => !current);
          if (!settingsActive) {
            onOpenStudentInfoSettings();
          }
        }}
      >
        <ListChecks size={17} />
        <span>Global Settings</span>
        {settingsExpanded ? <ChevronDown size={15} /> : <ChevronRight size={15} />}
      </button>

      {settingsExpanded ? (
        <div className="sidebarChildGroup">
          <button
            type="button"
            className={`sidebarNavButton child ${activeView === 'student-info-settings' ? 'active' : ''}`}
            onClick={onOpenStudentInfoSettings}
          >
            <span>Student Info</span>
          </button>
          <button
            type="button"
            className={`sidebarNavButton child ${activeView === 'template-library-settings' ? 'active' : ''}`}
            onClick={() => setTemplateSettingsExpanded((current) => !current)}
          >
            <span>Template Settings</span>
            {templateSettingsExpanded ? <ChevronDown size={15} /> : <ChevronRight size={15} />}
          </button>
          {templateSettingsExpanded ? (
            <div className="sidebarChildGroup nested">
              <button
                type="button"
                className={`sidebarNavButton child ${activeView === 'template-library-settings' ? 'active' : ''}`}
                onClick={onOpenTemplateLibrarySettings}
              >
                <span>Template Library</span>
              </button>
            </div>
          ) : null}
        </div>
      ) : null}

      <button
        type="button"
        className={`sidebarNavButton ${activeView === 'batch' ? 'active' : ''}`}
        onClick={onOpenBatch}
      >
        <Files size={17} />
        <span>Batch</span>
      </button>

      <div className="studentList">
        {loading ? (
          <div className="sideNote">Loading...</div>
        ) : students.length === 0 ? (
          <div className="sideNote">No students</div>
        ) : (
          students.map((student) => {
            const draftName = studentDrafts[student.id] ?? student.display_name;
            const isDeleting = deleteMutation.isPending && deleteMutation.variables === student.id;
            return (
              <div
                key={student.id}
                className={`studentItem ${
                  activeView === 'student' && student.id === selectedStudentId ? 'active' : ''
                }`}
              >
                <button
                  type="button"
                  className="studentSelectButton"
                  onClick={() => onSelect(student.id)}
                  aria-label={`Open ${student.display_name}`}
                >
                  <UserRound size={17} />
                </button>
                <input
                  className={dirtyStudentNames[student.id] ? 'studentNameInput dirtyInput' : 'studentNameInput'}
                  value={draftName}
                  onFocus={() => onSelect(student.id)}
                  onChange={(event) => updateStudentDraft(student.id, event.target.value)}
                  onBlur={() => commitStudentName(student.id)}
                  aria-label={`Name for ${student.display_name || student.id}`}
                />
                <button
                  type="button"
                  className="iconButton danger studentDeleteButton"
                  onMouseDown={(event) => event.preventDefault()}
                  onClick={() => removeStudent(student)}
                  disabled={isDeleting}
                  aria-label={`Delete ${student.display_name}`}
                >
                  <Trash2 size={16} />
                </button>
              </div>
            );
          })
        )}
      </div>

      <form className="studentCreate" onSubmit={submit}>
        <input
          value={displayName}
          onChange={(event) => setDisplayName(event.target.value)}
          placeholder="Student name"
          aria-label="Student name"
        />
        <button type="submit" aria-label="Create student" disabled={createMutation.isPending}>
          <Plus size={18} />
        </button>
      </form>
      {patchMutation.error ? <div className="inlineError">{patchMutation.error.message}</div> : null}
      {deleteMutation.error ? <div className="inlineError">{deleteMutation.error.message}</div> : null}
    </aside>
  );
}

import { FormEvent, useState } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { Plus, UserRound } from 'lucide-react';
import { createStudent } from '../api/client';
import type { Student } from '../api/types';

type StudentSidebarProps = {
  students: Student[];
  selectedStudentId: string | null;
  loading: boolean;
  onSelect: (studentId: string) => void;
};

export function StudentSidebar({ students, selectedStudentId, loading, onSelect }: StudentSidebarProps) {
  const [displayName, setDisplayName] = useState('');
  const queryClient = useQueryClient();

  const createMutation = useMutation({
    mutationKey: ['create-student'],
    mutationFn: createStudent,
    onSuccess: (student) => {
      queryClient.setQueryData<Student[]>(['students'], (old = []) => [student, ...old]);
      setDisplayName('');
      onSelect(student.id);
    },
  });

  const submit = (event: FormEvent) => {
    event.preventDefault();
    createMutation.mutate(displayName.trim() || 'Unnamed student');
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

      <div className="studentList">
        {loading ? (
          <div className="sideNote">Loading...</div>
        ) : students.length === 0 ? (
          <div className="sideNote">No students</div>
        ) : (
          students.map((student) => (
            <button
              key={student.id}
              className={`studentItem ${student.id === selectedStudentId ? 'active' : ''}`}
              onClick={() => onSelect(student.id)}
            >
              <UserRound size={17} />
              <span>{student.display_name}</span>
            </button>
          ))
        )}
      </div>
    </aside>
  );
}

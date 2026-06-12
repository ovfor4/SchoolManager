import { useEffect, useState } from 'react';
import { useIsMutating, useQuery } from '@tanstack/react-query';
import { RefreshCw, Wifi, WifiOff } from 'lucide-react';
import { getStudent, listStudents } from './api/client';
import { useStudentSocket } from './api/useStudentSocket';
import { FilePanel } from './components/FilePanel';
import { GradeTable } from './components/GradeTable';
import { StudentSidebar } from './components/StudentSidebar';

export function App() {
  const [selectedStudentId, setSelectedStudentId] = useState<string | null>(null);
  const savingCount = useIsMutating();

  const studentsQuery = useQuery({
    queryKey: ['students'],
    queryFn: listStudents,
  });

  useEffect(() => {
    if (!selectedStudentId && studentsQuery.data?.length) {
      setSelectedStudentId(studentsQuery.data[0].id);
    }
  }, [selectedStudentId, studentsQuery.data]);

  const detailQuery = useQuery({
    queryKey: ['student', selectedStudentId],
    queryFn: () => getStudent(selectedStudentId!),
    enabled: Boolean(selectedStudentId),
  });

  const socketStatus = useStudentSocket(selectedStudentId);

  return (
    <main className="appShell">
      <StudentSidebar
        students={studentsQuery.data ?? []}
        selectedStudentId={selectedStudentId}
        loading={studentsQuery.isLoading}
        onSelect={setSelectedStudentId}
      />

      <section className="workArea">
        <header className="topBar">
          <div>
            <h1>{detailQuery.data?.student.display_name ?? 'SchoolManager'}</h1>
            <p>{detailQuery.data?.student.folder_path ?? 'Student grade workspace'}</p>
          </div>
          <div className="statusCluster">
            <span className={`statusPill ${socketStatus === 'connected' ? 'ok' : 'muted'}`}>
              {socketStatus === 'connected' ? <Wifi size={16} /> : <WifiOff size={16} />}
              {socketStatus === 'connected' ? 'Live' : 'Offline'}
            </span>
            <span className={`statusPill ${savingCount > 0 ? 'saving' : 'ok'}`}>
              <RefreshCw size={16} className={savingCount > 0 ? 'spin' : ''} />
              {savingCount > 0 ? 'Saving' : 'Saved'}
            </span>
          </div>
        </header>

        {!selectedStudentId ? (
          <div className="emptyState">Create a student to start.</div>
        ) : detailQuery.isLoading ? (
          <div className="emptyState">Loading student data...</div>
        ) : detailQuery.error ? (
          <div className="errorState">{detailQuery.error.message}</div>
        ) : detailQuery.data ? (
          <div className="contentStack">
            <GradeTable studentId={selectedStudentId} grades={detailQuery.data.grades} />
            <FilePanel studentId={selectedStudentId} files={detailQuery.data.files} />
          </div>
        ) : null}
      </section>
    </main>
  );
}

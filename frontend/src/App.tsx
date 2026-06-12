import { useEffect, useState } from 'react';
import { useIsMutating, useQuery } from '@tanstack/react-query';
import { RefreshCw, Wifi, WifiOff } from 'lucide-react';
import { getStudent, listStudents } from './api/client';
import { useSchoolSocket } from './api/useSchoolSocket';
import { useStudentSocket } from './api/useStudentSocket';
import { FilePanel } from './components/FilePanel';
import { GradeTable } from './components/GradeTable';
import { StudentInfoDefinitionsPage } from './components/StudentInfoDefinitionsPage';
import { StudentInfoPanel } from './components/StudentInfoPanel';
import { AppView, StudentSidebar } from './components/StudentSidebar';

export function App() {
  const [selectedStudentId, setSelectedStudentId] = useState<string | null>(null);
  const [activeView, setActiveView] = useState<AppView>('student');
  const savingCount = useIsMutating();

  const studentsQuery = useQuery({
    queryKey: ['students'],
    queryFn: listStudents,
  });

  useEffect(() => {
    if (!studentsQuery.data) {
      return;
    }
    if (studentsQuery.data.length === 0) {
      setSelectedStudentId(null);
      return;
    }
    const selectedStudentExists = studentsQuery.data.some((student) => student.id === selectedStudentId);
    if (!selectedStudentId || !selectedStudentExists) {
      setSelectedStudentId(studentsQuery.data[0].id);
    }
  }, [selectedStudentId, studentsQuery.data]);

  const detailQuery = useQuery({
    queryKey: ['student', selectedStudentId],
    queryFn: () => getStudent(selectedStudentId!),
    enabled: activeView === 'student' && Boolean(selectedStudentId),
  });

  const schoolSocketStatus = useSchoolSocket();
  const studentSocketStatus = useStudentSocket(activeView === 'student' ? selectedStudentId : null);
  const socketStatus =
    schoolSocketStatus === 'connected' || studentSocketStatus === 'connected' ? 'connected' : 'reconnecting';

  const openStudent = (studentId: string | null) => {
    setActiveView('student');
    setSelectedStudentId(studentId);
  };

  return (
    <main className="appShell">
      <StudentSidebar
        students={studentsQuery.data ?? []}
        selectedStudentId={selectedStudentId}
        activeView={activeView}
        loading={studentsQuery.isLoading}
        onSelect={openStudent}
        onOpenGlobalSettings={() => setActiveView('global-settings')}
      />

      <section className="workArea">
        <header className="topBar">
          <div>
            <h1>
              {activeView === 'global-settings'
                ? 'Global Settings'
                : detailQuery.data?.student.display_name ?? 'SchoolManager'}
            </h1>
            <p>
              {activeView === 'global-settings'
                ? 'School-wide configuration'
                : detailQuery.data?.student.folder_path ?? 'Student grade workspace'}
            </p>
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

        {activeView === 'global-settings' ? (
          <StudentInfoDefinitionsPage />
        ) : !selectedStudentId ? (
          <div className="emptyState">Create a student to start.</div>
        ) : detailQuery.isLoading ? (
          <div className="emptyState">Loading student data...</div>
        ) : detailQuery.error ? (
          <div className="errorState">{detailQuery.error.message}</div>
        ) : detailQuery.data ? (
          <div className="contentStack">
            <StudentInfoPanel studentId={selectedStudentId} infoFields={detailQuery.data.info_fields} />
            <GradeTable studentId={selectedStudentId} grades={detailQuery.data.grades} />
            <FilePanel studentId={selectedStudentId} files={detailQuery.data.files} />
          </div>
        ) : null}
      </section>
    </main>
  );
}

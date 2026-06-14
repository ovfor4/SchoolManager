import { useEffect, useState } from 'react';
import { useIsMutating, useQuery } from '@tanstack/react-query';
import { RefreshCw, Wifi, WifiOff } from 'lucide-react';
import { getStudent, listStudents } from './api/client';
import { useSchoolSocket } from './api/useSchoolSocket';
import { useStudentSocket } from './api/useStudentSocket';
import { BatchPage } from './components/BatchPage';
import { FileManager } from './components/FileManager';
import { GradeTable } from './components/GradeTable';
import { StudentInfoDefinitionsPage } from './components/StudentInfoDefinitionsPage';
import { StudentInfoPanel } from './components/StudentInfoPanel';
import { AppView, StudentSidebar } from './components/StudentSidebar';
import {
  defaultGlobalTemplatesContextId,
  globalTemplatesContextType,
  studentUploadsContextType,
} from './config/constants';

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
  const pageTitle =
    activeView === 'student-info-settings'
      ? 'Student Info'
      : activeView === 'template-library-settings'
        ? 'Template Library'
      : activeView === 'batch'
        ? 'Batch'
        : detailQuery.data?.student.display_name ?? 'SchoolManager';
  const pageSubtitle =
    activeView === 'student-info-settings'
      ? 'Global settings'
      : activeView === 'template-library-settings'
        ? 'Template settings'
      : activeView === 'batch'
        ? 'Template generation'
        : detailQuery.data?.student.id ?? 'Student grade workspace';

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
        onOpenStudentInfoSettings={() => setActiveView('student-info-settings')}
        onOpenTemplateLibrarySettings={() => setActiveView('template-library-settings')}
        onOpenBatch={() => setActiveView('batch')}
      />

      <section className="workArea">
        <header className="topBar">
          <div>
            <h1>
              {pageTitle}
            </h1>
            <p>{pageSubtitle}</p>
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

        {activeView === 'student-info-settings' ? (
          <StudentInfoDefinitionsPage />
        ) : activeView === 'template-library-settings' ? (
          <FileManager
            context={{
              type: globalTemplatesContextType,
              id: defaultGlobalTemplatesContextId,
            }}
            title="Template Library"
            rootLabel="Templates"
            emptyLabel="No templates"
          />
        ) : activeView === 'batch' ? (
          <BatchPage students={studentsQuery.data ?? []} loading={studentsQuery.isLoading} />
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
            <FileManager context={{ type: studentUploadsContextType, id: selectedStudentId }} />
          </div>
        ) : null}
      </section>
    </main>
  );
}

import { useEffect, useMemo, useState } from 'react';
import { useMutation } from '@tanstack/react-query';
import { Download, FileCheck2 } from 'lucide-react';
import { generateFileTemplates } from '../api/client';
import type { FileContext, FileEntry, GeneratedFileDownload, Student } from '../api/types';
import {
  defaultGlobalTemplatesContextId,
  globalTemplatesContextType,
} from '../config/constants';
import { FileManager } from './FileManager';

type BatchActionId = 'none' | 'template';

type BatchAction = {
  id: BatchActionId;
  label: string;
};

type BatchPageProps = {
  students: Student[];
  loading: boolean;
};

const batchActions: BatchAction[] = [
  {
    id: 'none',
    label: 'None',
  },
  {
    id: 'template',
    label: 'Template',
  },
];

const templateContext: FileContext = {
  type: globalTemplatesContextType,
  id: defaultGlobalTemplatesContextId,
};

const downloadGeneratedFile = (download: GeneratedFileDownload) => {
  const url = URL.createObjectURL(download.blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = download.fileName;
  document.body.appendChild(link);
  link.click();
  link.remove();
  URL.revokeObjectURL(url);
};

export function BatchPage({ students, loading }: BatchPageProps) {
  const [actionId, setActionId] = useState<BatchActionId>('none');
  const [selectedStudentIds, setSelectedStudentIds] = useState<string[]>([]);
  const [selectedTemplate, setSelectedTemplate] = useState<FileEntry | null>(null);

  useEffect(() => {
    const currentStudentIds = new Set(students.map((student) => student.id));
    setSelectedStudentIds((current) => current.filter((studentId) => currentStudentIds.has(studentId)));
  }, [students]);

  const selectedStudents = useMemo(() => {
    const selected = new Set(selectedStudentIds);
    return students.filter((student) => selected.has(student.id));
  }, [selectedStudentIds, students]);

  const generateMutation = useMutation({
    mutationKey: ['file-template-generate'],
    mutationFn: () => {
      if (!selectedTemplate) {
        throw new Error('Template is required');
      }
      return generateFileTemplates({
        template_entry_id: selectedTemplate.id,
        student_ids: selectedStudentIds,
      });
    },
    onSuccess: downloadGeneratedFile,
  });

  const toggleStudent = (studentId: string) => {
    setSelectedStudentIds((current) =>
      current.includes(studentId)
        ? current.filter((item) => item !== studentId)
        : [...current, studentId],
    );
  };

  const selectAllStudents = () => setSelectedStudentIds(students.map((student) => student.id));
  const clearStudents = () => setSelectedStudentIds([]);
  const canGenerate = selectedStudentIds.length > 0 && Boolean(selectedTemplate);

  return (
    <div className="contentStack batchPage">
      <section className="workspaceSection">
        <div className="sectionHeader">
          <div>
            <h2>Batch</h2>
            <span>{actionId === 'template' ? `${selectedStudents.length} selected` : 'No operation'}</span>
          </div>
        </div>

        <div className="batchOperationRow">
          <div className="batchPanel">
            <label className="fieldLabel">
              <span>Operation</span>
              <select value={actionId} onChange={(event) => setActionId(event.target.value as BatchActionId)}>
                {batchActions.map((action) => (
                  <option key={action.id} value={action.id}>
                    {action.label}
                  </option>
                ))}
              </select>
            </label>
          </div>
        </div>

        {actionId === 'template' ? (
          <div className="batchGrid">
            <div className="batchPanel">
              <div className="batchSelectionHeader">
                <strong>Students</strong>
                <div>
                  <button type="button" className="textButton" onClick={selectAllStudents} disabled={students.length === 0}>
                    All
                  </button>
                  <button type="button" className="textButton" onClick={clearStudents} disabled={selectedStudentIds.length === 0}>
                    Clear
                  </button>
                </div>
              </div>

              <div className="studentCheckList">
                {loading ? (
                  <div className="sideNote">Loading...</div>
                ) : students.length === 0 ? (
                  <div className="sideNote">No students</div>
                ) : (
                  students.map((student) => (
                    <label key={student.id} className="studentCheckRow">
                      <input
                        type="checkbox"
                        checked={selectedStudentIds.includes(student.id)}
                        onChange={() => toggleStudent(student.id)}
                      />
                      <span>{student.display_name}</span>
                      <small>{student.id}</small>
                    </label>
                  ))
                )}
              </div>
            </div>

            <div className="batchPanel">
              <div className="selectedTemplateRow">
                <FileCheck2 size={18} />
                <span>{selectedTemplate?.name ?? 'No template selected'}</span>
              </div>
              <button
                type="button"
                className="iconTextButton"
                onClick={() => generateMutation.mutate()}
                disabled={!canGenerate || generateMutation.isPending}
              >
                <Download size={18} />
                {generateMutation.isPending ? 'Generating' : 'Generate'}
              </button>
              {generateMutation.error ? (
                <div className="inlineError">{generateMutation.error.message}</div>
              ) : null}
            </div>
          </div>
        ) : null}
      </section>

      {actionId === 'template' ? (
        <FileManager
          context={templateContext}
          title="Template Library"
          rootLabel="Templates"
          emptyLabel="No templates"
          readOnly
          selectedFileId={selectedTemplate?.id ?? null}
          onSelectFile={setSelectedTemplate}
        />
      ) : null}
    </div>
  );
}

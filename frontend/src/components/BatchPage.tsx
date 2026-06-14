import { useEffect, useMemo, useState } from 'react';
import { useMutation, useQuery } from '@tanstack/react-query';
import { Download, FileCheck2 } from 'lucide-react';
import { generateFileTemplates, getFileTemplateCapabilities } from '../api/client';
import {
  defaultExportFormatForSource,
  detectDocumentSourceFormat,
  exportFormatsForSource,
  exportFormatById,
  sourceFormatById,
} from '../api/documentFormats';
import type {
  FileContext,
  FileEntry,
  GeneratedFileDownload,
  Student,
} from '../api/types';
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
  const [sourceFormatId, setSourceFormatId] = useState('');
  const [exportFormatId, setExportFormatId] = useState('');
  const [matchedTemplateKey, setMatchedTemplateKey] = useState('');

  const capabilitiesQuery = useQuery({
    queryKey: ['file-template-capabilities'],
    queryFn: getFileTemplateCapabilities,
  });

  const sourceFormats = capabilitiesQuery.data?.source_formats ?? [];
  const selectedTemplateMatchKey = selectedTemplate
    ? `${selectedTemplate.id}:${selectedTemplate.name}:${selectedTemplate.mime_type}`
    : '__none__';

  useEffect(() => {
    const currentStudentIds = new Set(students.map((student) => student.id));
    setSelectedStudentIds((current) => current.filter((studentId) => currentStudentIds.has(studentId)));
  }, [students]);

  useEffect(() => {
    if (!capabilitiesQuery.data || matchedTemplateKey === selectedTemplateMatchKey) {
      return;
    }
    setSourceFormatId(detectDocumentSourceFormat(capabilitiesQuery.data, selectedTemplate)?.id ?? '');
    setMatchedTemplateKey(selectedTemplateMatchKey);
  }, [capabilitiesQuery.data, matchedTemplateKey, selectedTemplate, selectedTemplateMatchKey]);

  const selectedSourceFormat = useMemo(
    () =>
      capabilitiesQuery.data
        ? sourceFormatById(capabilitiesQuery.data, sourceFormatId)
        : null,
    [capabilitiesQuery.data, sourceFormatId],
  );

  const exportOptions = useMemo(
    () =>
      capabilitiesQuery.data
        ? exportFormatsForSource(capabilitiesQuery.data, selectedSourceFormat)
        : [],
    [capabilitiesQuery.data, selectedSourceFormat],
  );

  useEffect(() => {
    if (!capabilitiesQuery.data || !selectedSourceFormat) {
      setExportFormatId('');
      return;
    }
    const defaultExport = defaultExportFormatForSource(capabilitiesQuery.data, selectedSourceFormat);
    setExportFormatId((current) => {
      const currentIsValid = selectedSourceFormat.export_formats.includes(current);
      return currentIsValid ? current : defaultExport?.id ?? '';
    });
  }, [capabilitiesQuery.data, selectedSourceFormat]);

  const selectedExportFormat = useMemo(
    () =>
      capabilitiesQuery.data
        ? exportFormatById(capabilitiesQuery.data, exportFormatId)
        : null,
    [capabilitiesQuery.data, exportFormatId],
  );

  const selectedStudents = useMemo(() => {
    const selected = new Set(selectedStudentIds);
    return students.filter((student) => selected.has(student.id));
  }, [selectedStudentIds, students]);

  const requiresStudents = selectedSourceFormat?.supports_student_variables ?? false;

  const generateMutation = useMutation({
    mutationKey: ['file-template-generate'],
    mutationFn: () => {
      if (!selectedTemplate) {
        throw new Error('Template is required');
      }
      if (!selectedSourceFormat || !selectedExportFormat) {
        throw new Error('Template format is required');
      }
      return generateFileTemplates({
        template_entry_id: selectedTemplate.id,
        student_ids: requiresStudents ? selectedStudentIds : undefined,
        source_format: selectedSourceFormat.id,
        export_format: selectedExportFormat.id,
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
  const canGenerate =
    Boolean(selectedTemplate) &&
    Boolean(selectedSourceFormat) &&
    Boolean(selectedExportFormat) &&
    (!requiresStudents || selectedStudentIds.length > 0);
  const actionLabel = requiresStudents ? 'Generate' : 'Download';
  const pendingActionLabel = requiresStudents ? 'Generating' : 'Downloading';

  return (
    <div className="contentStack batchPage">
      <section className="workspaceSection">
        <div className="sectionHeader">
          <div>
            <h2>Batch</h2>
            <span>
              {actionId === 'template' && requiresStudents
                ? `${selectedStudents.length} selected`
                : actionId === 'template'
                  ? 'Template selected'
                  : 'No operation'}
            </span>
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
            {requiresStudents ? (
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
            ) : null}

            <div className="batchPanel">
              <div className="templateFormatGrid">
                <label className="fieldLabel">
                  <span>Source Format</span>
                  <select
                    value={sourceFormatId}
                    onChange={(event) => setSourceFormatId(event.target.value)}
                    disabled={sourceFormats.length === 0}
                  >
                    {sourceFormats.map((format) => (
                      <option key={format.id} value={format.id}>
                        {format.label}
                      </option>
                    ))}
                  </select>
                </label>
                <label className="fieldLabel">
                  <span>Export Format</span>
                  <select
                    value={exportFormatId}
                    onChange={(event) => setExportFormatId(event.target.value)}
                    disabled={exportOptions.length === 0}
                  >
                    {exportOptions.map((format) => (
                      <option key={format.id} value={format.id}>
                        {format.label}
                      </option>
                    ))}
                  </select>
                </label>
              </div>
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
                {generateMutation.isPending ? pendingActionLabel : actionLabel}
              </button>
              {capabilitiesQuery.error ? (
                <div className="inlineError">{capabilitiesQuery.error.message}</div>
              ) : null}
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

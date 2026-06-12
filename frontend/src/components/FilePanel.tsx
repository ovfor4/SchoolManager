import { ChangeEvent, useRef, useState } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { FileText, Upload } from 'lucide-react';
import { downloadUrl, uploadFile } from '../api/client';
import type { StoredFile, StudentDetail } from '../api/types';

type FilePanelProps = {
  studentId: string;
  files: StoredFile[];
};

export function FilePanel({ studentId, files }: FilePanelProps) {
  const inputRef = useRef<HTMLInputElement | null>(null);
  const queryClient = useQueryClient();
  const [progress, setProgress] = useState<number | null>(null);

  const uploadMutation = useMutation({
    mutationKey: ['upload-file', studentId],
    mutationFn: (file: File) => uploadFile(studentId, file, setProgress),
    onSuccess: (file) => {
      queryClient.setQueryData<StudentDetail>(['student', studentId], (old) =>
        old ? { ...old, files: old.files.some((item) => item.id === file.id) ? old.files : [file, ...old.files] } : old,
      );
      setProgress(null);
      if (inputRef.current) {
        inputRef.current.value = '';
      }
    },
    onError: () => setProgress(null),
  });

  const chooseFile = (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      setProgress(0);
      uploadMutation.mutate(file);
    }
  };

  return (
    <section className="workspaceSection">
      <div className="sectionHeader">
        <div>
          <h2>Files</h2>
          <span>{files.length} uploads</span>
        </div>
        <button className="iconTextButton" onClick={() => inputRef.current?.click()} disabled={uploadMutation.isPending}>
          <Upload size={18} />
          Upload
        </button>
        <input ref={inputRef} type="file" className="hiddenFileInput" onChange={chooseFile} />
      </div>

      {progress !== null ? (
        <div className="uploadProgress" aria-label="Upload progress">
          <span style={{ width: `${progress}%` }} />
        </div>
      ) : null}

      {uploadMutation.error ? <div className="inlineError">{uploadMutation.error.message}</div> : null}

      <div className="fileList">
        {files.length === 0 ? (
          <div className="emptyCell">No uploaded files</div>
        ) : (
          files.map((file) => (
            <a key={file.id} className="fileRow" href={downloadUrl(studentId, file.id)}>
              <FileText size={18} />
              <span>{file.original_name}</span>
              <small>{Math.max(1, Math.round(file.size_bytes / 1024))} KB</small>
            </a>
          ))
        )}
      </div>
    </section>
  );
}

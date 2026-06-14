import { ChangeEvent, useEffect, useRef, useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import {
  createFolder,
  listFileEntries,
  listTrash,
  moveEntryToTrash,
  permanentlyDeleteTrashEntry,
  renameEntry,
  restoreTrashEntry,
  uploadFileEntry,
} from '../api/client';
import {
  fileManagerEntriesKey,
  fileManagerEntriesPrefix,
  fileManagerTrashKey,
} from '../api/fileManagerKeys';
import type { FileContext, FileEntry, TrashEntry } from '../api/types';
import { CreateFolderDialog } from './CreateFolderDialog';
import { FileEntryTable } from './FileEntryTable';
import { FileManagerToolbar } from './FileManagerToolbar';
import { RenameDialog } from './RenameDialog';
import { TrashView } from './TrashView';

type FileManagerProps = {
  context: FileContext;
  title?: string;
  rootLabel?: string;
  emptyLabel?: string;
  readOnly?: boolean;
  selectedFileId?: string | null;
  onSelectFile?: (entry: FileEntry) => void;
};

const errorMessage = (error: unknown) => (error instanceof Error ? error.message : undefined);

export function FileManager({
  context,
  title = 'Files',
  rootLabel = 'Files',
  emptyLabel = 'No files',
  readOnly = false,
  selectedFileId,
  onSelectFile,
}: FileManagerProps) {
  const queryClient = useQueryClient();
  const inputRef = useRef<HTMLInputElement | null>(null);
  const [path, setPath] = useState<FileEntry[]>([]);
  const [view, setView] = useState<'files' | 'trash'>('files');
  const [progress, setProgress] = useState<number | null>(null);
  const [folderDialogOpen, setFolderDialogOpen] = useState(false);
  const [renamingEntry, setRenamingEntry] = useState<FileEntry | null>(null);

  useEffect(() => {
    setPath([]);
    setView('files');
  }, [context.type, context.id]);

  useEffect(() => {
    if (readOnly) {
      setView('files');
    }
  }, [readOnly]);

  const parentId = path.length > 0 ? path[path.length - 1].id : null;

  const fileQuery = useQuery({
    queryKey: fileManagerEntriesKey(context, parentId),
    queryFn: () => listFileEntries(context, parentId),
    enabled: view === 'files',
  });

  const trashQuery = useQuery({
    queryKey: fileManagerTrashKey(context),
    queryFn: () => listTrash(context),
    enabled: !readOnly && view === 'trash',
  });

  const invalidateFiles = () => {
    void queryClient.invalidateQueries({ queryKey: fileManagerEntriesPrefix(context) });
    void queryClient.invalidateQueries({ queryKey: fileManagerTrashKey(context) });
  };

  const uploadMutation = useMutation({
    mutationKey: ['file-manager-upload', context.type, context.id, parentId],
    mutationFn: (file: File) => uploadFileEntry(context, parentId, file, setProgress),
    onSuccess: () => {
      invalidateFiles();
      setProgress(null);
      if (inputRef.current) {
        inputRef.current.value = '';
      }
    },
    onError: () => setProgress(null),
  });

  const createFolderMutation = useMutation({
    mutationKey: ['file-manager-create-folder', context.type, context.id, parentId],
    mutationFn: (name: string) => createFolder(context, parentId, name),
    onSuccess: () => {
      setFolderDialogOpen(false);
      invalidateFiles();
    },
  });

  const renameMutation = useMutation({
    mutationKey: ['file-manager-rename', context.type, context.id],
    mutationFn: ({ entry, name }: { entry: FileEntry; name: string }) => renameEntry(context, entry.id, name),
    onSuccess: (entry) => {
      setRenamingEntry(null);
      setPath((current) => current.map((item) => (item.id === entry.id ? entry : item)));
      invalidateFiles();
    },
  });

  const trashMutation = useMutation({
    mutationKey: ['file-manager-trash-entry', context.type, context.id],
    mutationFn: (entry: FileEntry) => moveEntryToTrash(context, entry.id),
    onSuccess: () => invalidateFiles(),
  });

  const restoreMutation = useMutation({
    mutationKey: ['file-manager-restore-trash', context.type, context.id],
    mutationFn: (entry: TrashEntry) => restoreTrashEntry(context, entry.id),
    onSuccess: () => invalidateFiles(),
  });

  const permanentDeleteMutation = useMutation({
    mutationKey: ['file-manager-delete-trash', context.type, context.id],
    mutationFn: (entry: TrashEntry) => permanentlyDeleteTrashEntry(context, entry.id),
    onSuccess: () => invalidateFiles(),
  });

  const chooseFile = (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      setProgress(0);
      uploadMutation.mutate(file);
    }
  };

  const openPathIndex = (index: number) => {
    setView('files');
    setPath(index < 0 ? [] : path.slice(0, index + 1));
  };

  const error =
    errorMessage(fileQuery.error) ??
    errorMessage(trashQuery.error) ??
    errorMessage(uploadMutation.error) ??
    errorMessage(trashMutation.error) ??
    errorMessage(restoreMutation.error) ??
    errorMessage(permanentDeleteMutation.error);

  return (
    <section className="workspaceSection fileManager">
      <div className="sectionHeader">
        <div>
          <h2>{title}</h2>
          <span>{view === 'files' ? `${fileQuery.data?.length ?? 0} entries` : `${trashQuery.data?.length ?? 0} trash`}</span>
        </div>
      </div>

      <FileManagerToolbar
        view={view}
        path={path}
        rootLabel={rootLabel}
        readOnly={readOnly}
        uploading={uploadMutation.isPending}
        onUpload={() => inputRef.current?.click()}
        onCreateFolder={() => setFolderDialogOpen(true)}
        onOpenFiles={() => setView('files')}
        onOpenTrash={() => setView('trash')}
        onOpenPathIndex={openPathIndex}
      />
      <input ref={inputRef} type="file" className="hiddenFileInput" onChange={chooseFile} />

      {progress !== null ? (
        <div className="uploadProgress" aria-label="Upload progress">
          <span style={{ width: `${progress}%` }} />
        </div>
      ) : null}

      {error ? <div className="inlineError">{error}</div> : null}

      {view === 'files' ? (
        <FileEntryTable
          context={context}
          entries={fileQuery.data ?? []}
          loading={fileQuery.isLoading}
          emptyLabel={emptyLabel}
          readOnly={readOnly}
          selectedFileId={selectedFileId}
          onSelectFile={onSelectFile}
          onOpenFolder={(entry) => setPath((current) => [...current, entry])}
          onRename={setRenamingEntry}
          onTrash={(entry) => trashMutation.mutate(entry)}
        />
      ) : (
        <TrashView
          trash={trashQuery.data ?? []}
          loading={trashQuery.isLoading}
          onRestore={(entry) => restoreMutation.mutate(entry)}
          onDelete={(entry) => permanentDeleteMutation.mutate(entry)}
        />
      )}

      <CreateFolderDialog
        open={folderDialogOpen}
        pending={createFolderMutation.isPending}
        error={errorMessage(createFolderMutation.error)}
        onClose={() => setFolderDialogOpen(false)}
        onSubmit={(name) => createFolderMutation.mutate(name)}
      />
      <RenameDialog
        entry={renamingEntry}
        pending={renameMutation.isPending}
        error={errorMessage(renameMutation.error)}
        onClose={() => setRenamingEntry(null)}
        onSubmit={(name) => {
          if (renamingEntry) {
            renameMutation.mutate({ entry: renamingEntry, name });
          }
        }}
      />
    </section>
  );
}

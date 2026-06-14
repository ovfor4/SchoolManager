import { ChevronRight, FolderPlus, Upload } from 'lucide-react';
import type { FileEntry } from '../api/types';

type FileManagerToolbarProps = {
  view: 'files' | 'trash';
  path: FileEntry[];
  rootLabel: string;
  readOnly: boolean;
  uploading: boolean;
  onUpload: () => void;
  onCreateFolder: () => void;
  onOpenFiles: () => void;
  onOpenTrash: () => void;
  onOpenPathIndex: (index: number) => void;
};

export function FileManagerToolbar({
  view,
  path,
  rootLabel,
  readOnly,
  uploading,
  onUpload,
  onCreateFolder,
  onOpenFiles,
  onOpenTrash,
  onOpenPathIndex,
}: FileManagerToolbarProps) {
  return (
    <div className="fileManagerToolbar">
      <div className="fileBreadcrumbs" aria-label="Folder path">
        <button type="button" className="breadcrumbButton" onClick={() => onOpenPathIndex(-1)}>
          {rootLabel}
        </button>
        {path.map((entry, index) => (
          <span key={entry.id} className="breadcrumbSegment">
            <ChevronRight size={15} />
            <button type="button" className="breadcrumbButton" onClick={() => onOpenPathIndex(index)}>
              {entry.name}
            </button>
          </span>
        ))}
      </div>
      {!readOnly ? (
        <div className="fileToolbarActions">
          <div className="segmentedControl" aria-label="File manager view">
            <button type="button" className={view === 'files' ? 'active' : ''} onClick={onOpenFiles}>
              Files
            </button>
            <button type="button" className={view === 'trash' ? 'active' : ''} onClick={onOpenTrash}>
              Trash
            </button>
          </div>
          <button className="iconTextButton" type="button" onClick={onCreateFolder} disabled={view !== 'files'}>
            <FolderPlus size={18} />
            Folder
          </button>
          <button className="iconTextButton" type="button" onClick={onUpload} disabled={view !== 'files' || uploading}>
            <Upload size={18} />
            Upload
          </button>
        </div>
      ) : null}
    </div>
  );
}

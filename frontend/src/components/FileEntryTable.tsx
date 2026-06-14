import { Check, Download, FileText, Folder, Pencil, Trash2 } from 'lucide-react';
import type { FileContext, FileEntry } from '../api/types';
import { downloadFileEntryUrl } from '../api/client';

type FileEntryTableProps = {
  context: FileContext;
  entries: FileEntry[];
  loading: boolean;
  emptyLabel?: string;
  readOnly?: boolean;
  selectedFileId?: string | null;
  onSelectFile?: (entry: FileEntry) => void;
  onOpenFolder: (entry: FileEntry) => void;
  onRename: (entry: FileEntry) => void;
  onTrash: (entry: FileEntry) => void;
};

const formatSize = (entry: FileEntry) => {
  if (entry.kind === 'folder') {
    return '--';
  }
  if (entry.size_bytes < 1024) {
    return `${entry.size_bytes} B`;
  }
  if (entry.size_bytes < 1024 * 1024) {
    return `${Math.max(1, Math.round(entry.size_bytes / 1024))} KB`;
  }
  return `${(entry.size_bytes / (1024 * 1024)).toFixed(1)} MB`;
};

const formatDate = (value: number) => new Date(value).toLocaleString();

export function FileEntryTable({
  context,
  entries,
  loading,
  emptyLabel = 'No files',
  readOnly = false,
  selectedFileId,
  onSelectFile,
  onOpenFolder,
  onRename,
  onTrash,
}: FileEntryTableProps) {
  return (
    <div className="tableFrame">
      <table className="fileTable">
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Size</th>
            <th>Updated</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {loading ? (
            <tr>
              <td className="emptyCell" colSpan={5}>
                Loading files...
              </td>
            </tr>
          ) : entries.length === 0 ? (
            <tr>
              <td className="emptyCell" colSpan={5}>
                {emptyLabel}
              </td>
            </tr>
          ) : (
            entries.map((entry) => (
              <tr key={entry.id}>
                <td>
                  {entry.kind === 'folder' ? (
                    <button type="button" className="fileNameButton" onClick={() => onOpenFolder(entry)}>
                      <Folder size={18} />
                      <span>{entry.name}</span>
                    </button>
                  ) : (
                    <a className="fileNameLink" href={downloadFileEntryUrl(context, entry.id)}>
                      <FileText size={18} />
                      <span>{entry.name}</span>
                    </a>
                  )}
                </td>
                <td>{entry.kind === 'folder' ? 'Folder' : entry.mime_type || 'File'}</td>
                <td>{formatSize(entry)}</td>
                <td>{formatDate(entry.updated_at)}</td>
                <td className="fileActions">
                  {entry.kind === 'file' && onSelectFile ? (
                    <button
                      type="button"
                      className={selectedFileId === entry.id ? 'iconButton active' : 'iconButton'}
                      onClick={() => onSelectFile(entry)}
                      aria-label={`Select ${entry.name}`}
                    >
                      <Check size={17} />
                    </button>
                  ) : null}
                  {entry.kind === 'file' ? (
                    <a
                      className="iconButton"
                      href={downloadFileEntryUrl(context, entry.id)}
                      aria-label={`Download ${entry.name}`}
                    >
                      <Download size={17} />
                    </a>
                  ) : null}
                  {!readOnly ? (
                    <>
                      <button
                        type="button"
                        className="iconButton"
                        onClick={() => onRename(entry)}
                        aria-label={`Rename ${entry.name}`}
                      >
                        <Pencil size={17} />
                      </button>
                      <button
                        type="button"
                        className="iconButton danger"
                        onClick={() => onTrash(entry)}
                        aria-label={`Move ${entry.name} to trash`}
                      >
                        <Trash2 size={17} />
                      </button>
                    </>
                  ) : null}
                </td>
              </tr>
            ))
          )}
        </tbody>
      </table>
    </div>
  );
}

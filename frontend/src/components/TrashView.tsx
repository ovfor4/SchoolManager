import { FileText, Folder, RotateCcw, Trash2 } from 'lucide-react';
import type { TrashEntry } from '../api/types';

type TrashViewProps = {
  trash: TrashEntry[];
  loading: boolean;
  onRestore: (entry: TrashEntry) => void;
  onDelete: (entry: TrashEntry) => void;
};

const formatDate = (value: number) => new Date(value).toLocaleString();

export function TrashView({ trash, loading, onRestore, onDelete }: TrashViewProps) {
  return (
    <div className="tableFrame">
      <table className="fileTable">
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Items</th>
            <th>Trashed</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {loading ? (
            <tr>
              <td className="emptyCell" colSpan={5}>
                Loading trash...
              </td>
            </tr>
          ) : trash.length === 0 ? (
            <tr>
              <td className="emptyCell" colSpan={5}>
                Trash is empty
              </td>
            </tr>
          ) : (
            trash.map((entry) => (
              <tr key={entry.id}>
                <td>
                  <span className="fileNameStatic">
                    {entry.root_kind === 'folder' ? <Folder size={18} /> : <FileText size={18} />}
                    <span>{entry.root_name}</span>
                  </span>
                </td>
                <td>{entry.root_kind === 'folder' ? 'Folder' : 'File'}</td>
                <td>{entry.item_count}</td>
                <td>{formatDate(entry.trashed_at)}</td>
                <td className="fileActions">
                  <button
                    type="button"
                    className="iconButton"
                    onClick={() => onRestore(entry)}
                    aria-label={`Restore ${entry.root_name}`}
                  >
                    <RotateCcw size={17} />
                  </button>
                  <button
                    type="button"
                    className="iconButton danger"
                    onClick={() => onDelete(entry)}
                    aria-label={`Permanently delete ${entry.root_name}`}
                  >
                    <Trash2 size={17} />
                  </button>
                </td>
              </tr>
            ))
          )}
        </tbody>
      </table>
    </div>
  );
}

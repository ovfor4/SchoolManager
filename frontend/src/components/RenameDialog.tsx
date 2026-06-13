import { FormEvent, useEffect, useState } from 'react';
import { X } from 'lucide-react';
import type { FileEntry } from '../api/types';

type RenameDialogProps = {
  entry: FileEntry | null;
  pending: boolean;
  error?: string;
  onClose: () => void;
  onSubmit: (name: string) => void;
};

export function RenameDialog({ entry, pending, error, onClose, onSubmit }: RenameDialogProps) {
  const [name, setName] = useState('');

  useEffect(() => {
    setName(entry?.name ?? '');
  }, [entry]);

  if (!entry) {
    return null;
  }

  const submit = (event: FormEvent) => {
    event.preventDefault();
    onSubmit(name);
  };

  return (
    <div className="dialogBackdrop" role="presentation">
      <form className="dialogPanel" onSubmit={submit}>
        <div className="dialogHeader">
          <h3>Rename</h3>
          <button type="button" className="iconButton" onClick={onClose} aria-label="Close">
            <X size={18} />
          </button>
        </div>
        <label className="dialogField">
          <span>Name</span>
          <input value={name} onChange={(event) => setName(event.target.value)} autoFocus />
        </label>
        {error ? <div className="inlineError">{error}</div> : null}
        <div className="dialogActions">
          <button type="button" className="iconTextButton" onClick={onClose}>
            Cancel
          </button>
          <button type="submit" className="iconTextButton" disabled={pending || name.trim().length === 0}>
            Rename
          </button>
        </div>
      </form>
    </div>
  );
}

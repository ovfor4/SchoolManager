import { FormEvent, useEffect, useState } from 'react';
import { X } from 'lucide-react';

type CreateFolderDialogProps = {
  open: boolean;
  pending: boolean;
  error?: string;
  onClose: () => void;
  onSubmit: (name: string) => void;
};

export function CreateFolderDialog({
  open,
  pending,
  error,
  onClose,
  onSubmit,
}: CreateFolderDialogProps) {
  const [name, setName] = useState('');

  useEffect(() => {
    if (open) {
      setName('');
    }
  }, [open]);

  if (!open) {
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
          <h3>New Folder</h3>
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
            Create
          </button>
        </div>
      </form>
    </div>
  );
}

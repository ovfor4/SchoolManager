export type Student = {
  id: string;
  display_name: string;
  folder_path: string;
  created_at: number;
  updated_at: number;
};

export type Grade = {
  id: string;
  title: string;
  score: string;
  max_score: string;
  occurred_on: string;
  notes: string;
  updated_at: number;
};

export type StoredFile = {
  id: string;
  original_name: string;
  stored_name: string;
  mime_type: string;
  status: 'pending' | 'active';
  size_bytes: number;
  created_at: number;
  updated_at: number;
};

export type StudentDetail = {
  student: Student;
  grades: Grade[];
  files: StoredFile[];
};

export type GradePatch = Partial<Pick<Grade, 'title' | 'score' | 'max_score' | 'occurred_on' | 'notes'>>;

export type WsMessage =
  | {
      type: 'grade.created' | 'grade.updated';
      student_id: string;
      resource: 'grade';
      id: string;
      field_id: string;
      grade: Grade;
      changed_fields?: Array<keyof GradePatch>;
    }
  | {
      type: 'grade.deleted';
      student_id: string;
      resource: 'grade';
      id: string;
      field_id: string;
    }
  | {
      type: 'file.created';
      student_id: string;
      resource: 'file';
      id: string;
      field_id: string;
      file: StoredFile;
    };

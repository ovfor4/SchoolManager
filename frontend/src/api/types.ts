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

export type StudentInfoValueType = 'INTEGER' | 'STRING' | 'DATE';

export type StudentInfoDefinition = {
  id: string;
  name: string;
  display_name: string;
  value_type: StudentInfoValueType;
  created_at: number;
  updated_at: number;
};

export type StudentInfoField = {
  id: string;
  name: string;
  display_name: string;
  value_type: StudentInfoValueType;
  value: string;
  updated_at: number;
};

export type FileContext = {
  type: 'student_uploads';
  id: string;
};

export type FileEntry = {
  id: string;
  context_type: string;
  context_id: string;
  parent_id: string | null;
  kind: 'file' | 'folder';
  name: string;
  mime_type: string;
  size_bytes: number;
  status: 'pending' | 'active' | 'trashed';
  created_at: number;
  updated_at: number;
  trashed_at: number | null;
};

export type TrashEntry = {
  id: string;
  context_type: string;
  context_id: string;
  root_entry_id: string;
  original_parent_id: string | null;
  root_name: string;
  root_kind: 'file' | 'folder';
  item_count: number;
  trashed_at: number;
};

export type StudentDetail = {
  student: Student;
  info_fields: StudentInfoField[];
  grades: Grade[];
};

export type StudentPatch = Partial<Pick<Student, 'display_name'>>;

export type StudentInfoDefinitionPatch = Partial<Pick<StudentInfoDefinition, 'name' | 'display_name' | 'value_type'>>;

export type StudentInfoValuePatch = Pick<StudentInfoField, 'value'>;

export type GradePatch = Partial<Pick<Grade, 'title' | 'score' | 'max_score' | 'occurred_on' | 'notes'>>;

export type FileManagerAction =
  | 'entry.created'
  | 'entry.updated'
  | 'entry.trashed'
  | 'trash.restored'
  | 'trash.deleted';

export type WsMessage =
  | {
      type: 'student.created' | 'student.updated';
      resource: 'student';
      id: string;
      field_id: string;
      student: Student;
      changed_fields?: Array<keyof StudentPatch>;
    }
  | {
      type: 'student.deleted';
      resource: 'student';
      id: string;
      field_id: string;
    }
  | {
      type: 'student_info_definition.created' | 'student_info_definition.updated';
      resource: 'student_info_definition';
      id: string;
      field_id: string;
      info_field_definition: StudentInfoDefinition;
      changed_fields?: Array<keyof StudentInfoDefinitionPatch>;
    }
  | {
      type: 'student_info_definition.deleted';
      resource: 'student_info_definition';
      id: string;
      field_id: string;
    }
  | {
      type: 'student_info.updated';
      student_id: string;
      resource: 'student_info';
      id: string;
      field_id: string;
      info_field: StudentInfoField;
      changed_fields?: Array<keyof StudentInfoValuePatch>;
    }
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
      type: 'file_manager.changed';
      student_id: string;
      resource: 'file_manager';
      id: string;
      field_id: string;
      context_type: 'student_uploads';
      context_id: string;
      action: FileManagerAction;
    };

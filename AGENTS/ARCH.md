# SchoolManager Architecture

## Goal

SchoolManager manages generic student grade records, student information values, and uploaded files. Uploaded files are exposed through the global file manager with a `student_uploads` context. The system intentionally does not distinguish school grades, standardized scores, contest scores, or language scores at the storage/API level. They are all grade records.

## Repository Layout

- `backend/`: C++23 backend.
- `backend/include/schoolmanager/domain`: pure domain models and domain helpers.
- `backend/src/domain`: domain implementations.
- `backend/include/schoolmanager/application`: backend use cases and application-level aggregation.
- `backend/src/application`: application use-case implementations.
- `backend/include/schoolmanager/infra`: SQLite, file-system storage, and connection-pool interfaces.
- `backend/src/infra`: infrastructure implementations.
- `backend/include/schoolmanager/adapters`: HTTP, WebSocket, and JSON adapter interfaces.
- `backend/src/adapters`: Drogon controllers, JSON conversion, and broadcast implementation.
- `backend/openapi/openapi.json`: HTTP API contract.
- `frontend/`: React + Vite frontend.
- `frontend/src/api`: HTTP/WebSocket client code and API types.
- `frontend/src/config`: frontend runtime and deployment constants.
- `frontend/src/store`: local client state such as dirty-field tracking.
- `frontend/src/components`: UI components.

## Backend Layers

### Domain

The domain layer contains data shapes and pure helpers:

- `StudentSummary`
- `StudentInfoDefinition`
- `StudentInfoValue`
- `StudentInfoField`
- `Grade`
- `FileContext`
- `FileEntry`
- `TrashEntry`
- `StudentDetail`
- ID generation, timestamp helpers, and safe ID validation

This layer must not depend on Drogon, SQLite, or the file system.

### Application

The application layer owns backend use-case orchestration across repositories:

- `StudentUseCases`: student list/create/detail/update/delete workflows, including per-student database initialization after school-index creation.
- `StudentInfoUseCases`: global student information definition workflows, per-student value updates, definition deletion cleanup, and student-info field aggregation.
- `GradeUseCases`: grade list/create/update/delete workflows and school-index touch updates after grade changes.
- `StudentInfoFields`: pure aggregation helpers for combining global definitions with per-student values.

Application code may depend on domain and infra. It must not depend on Drogon, JSON, WebSocket adapters, or other external protocols.

### Infra

The infra layer owns persistence and storage details:

- `StoragePaths`: maps the configured data root into `school_index.db`, student folders, per-student `data.db`, and upload directories.
- `SqliteConnection`: RAII wrapper around `sqlite3*` and prepared statements.
- `LruSqlitePool`: bounded pool/cache for open SQLite connections.
- `SchoolIndexRepository`: reads and writes `school_index.db`, including school-wide student information field definitions.
- `StudentDataRepository`: reads and writes each student's `data.db`, including that student's student information values and grade records. It keeps the legacy `files` table only as a migration source.
- `FileManagerRepository`: reads and writes file manager metadata tables in the context owner's SQLite database.
- `FileStorage`: owns file-system paths and file moves for active uploads and trash.
- `FileManagerService`: validates file manager contexts and orchestrates upload, download, folder, rename, trash, restore, and permanent delete workflows.

SQLite connections enable WAL, foreign keys, `busy_timeout`, and normal synchronous mode when opened.

### Adapters

The adapters layer translates external protocols into domain/infra calls:

- `SystemController`: health and OpenAPI routes.
- `StudentsController`: student metadata and student detail HTTP routes.
- `StudentInfoController`: student information definition and value HTTP routes.
- `GradesController`: grade HTTP routes.
- `FileManagerController`: file manager HTTP routes.
- `StudentWebSocketController`: WebSocket room join and ping/pong handling.
- `RealtimePublisher`: WebSocket message construction for HTTP-triggered mutations.
- `BroadcastHub`: in-memory per-student WebSocket rooms.
- `JsonHelpers`: JSON serialization/deserialization helpers.

Adapters may depend on application, domain, and infra. Domain, application, and infra must not depend on adapters.

## Storage Model

The storage root defaults to `./runtime-data` unless `SM_DATA_ROOT` is set.

```text
runtime-data/
  school_index.db
  students/
    <student_id>/
      data.db
      uploads/
        <stored_file_name>
        .trash/
          <trash_uuid>-<name>/
```

`school_index.db` contains school-wide student metadata and global student information field definitions. It must not become a cross-student grade or student information value store.

Each student's `data.db` contains that student's student information values, grades, and file manager metadata for the `student_uploads` context. This keeps migration simple: moving a student means moving one folder.

Uploaded files are stored in the same student folder under `uploads/`. File manager folders are logical database entries; active folder renames do not move file bytes on disk. Deleted file bytes are moved under `uploads/.trash/<trash_uuid>-<name>/`.

## Write Flow

Student metadata edits and deletes use `school_index.db` only:

```text
Frontend student name blur -> PATCH student display_name -> school_index.db -> school WebSocket broadcast
Frontend delete student -> DELETE student -> school_index.db delete -> remove student folder -> school WebSocket broadcast
```

Global student information field definitions use `school_index.db` only:

```text
Frontend Global Settings -> POST/PATCH/DELETE student info definition -> school_index.db -> school WebSocket broadcast
```

Per-student information value edits use partial update semantics against that student's `data.db`:

```text
Frontend student info input blur -> PATCH student info value -> student data.db -> school_index touch -> student WebSocket broadcast
```

All students share the same global student information definitions. Student pages render those definitions directly as value inputs; they do not create, delete, or select field definitions.

Grade edits use partial update semantics:

```text
Frontend input blur -> PATCH grade fields -> student data.db -> school_index touch -> WebSocket broadcast
```

Only fields present in the PATCH request are updated.

File upload uses the global file manager API with `contextType=student_uploads` and `contextId=<student_id>`. The upload still uses an atomic activation sequence:

```text
write .tmp file -> insert DB pending record -> atomic rename -> mark DB active
```

`FileManagerService` chooses the temporary path and owns the activation sequence. The HTTP adapter only parses multipart data and supplies a temporary-file writer callback, so large uploads do not require duplicating the uploaded bytes in service code. If rename fails, the pending DB record and temporary file are removed.

File manager delete and restore operations are coordinated in `FileManagerService`, not controllers. Delete creates a trash entry, recursively marks the deleted subtree, and moves file bytes into a single trash directory. Restore moves file bytes back to the active upload root and returns `409` if the original parent is missing or a sibling name conflict exists.

## Realtime Model

Clients connect to:

```text
WS /api/ws/students?student_id=<student_id>
WS /api/ws/students?scope=school
```

The backend keeps in-memory WebSocket rooms by student ID for per-student data and a school-level room for the student list. Student metadata mutations broadcast `student.created`, `student.updated`, and `student.deleted` to the school room. Per-student mutations broadcast messages such as `grade.created`, `grade.updated`, and `grade.deleted` to the student room.

File manager mutations for `student_uploads` broadcast one `file_manager.changed` message to the matching student room. The message includes `context_type`, `context_id`, and an action such as `entry.created`, `entry.updated`, `entry.trashed`, `trash.restored`, or `trash.deleted`. The frontend invalidates the matching file manager entries and trash query keys instead of storing file state inside `StudentDetail`.

Global student information definition mutations broadcast `student_info_definition.created`, `student_info_definition.updated`, and `student_info_definition.deleted` to the school room. Per-student information value mutations broadcast `student_info.updated` to the relevant student room.

The frontend compares incoming `field_id`/`changed_fields` against its local dirty-field map. If the same field is being edited locally, the incoming UI update is ignored silently.

The frontend must deduplicate create events by resource ID because the client that made a mutation can receive both the HTTP mutation response and the WebSocket broadcast for the same resource.

## Deployment Model

Remote deployments expose the Vite frontend port only. The frontend defaults to listening on `0.0.0.0:5173`, while the backend defaults to listening on `127.0.0.1:8080`.

Browser HTTP and WebSocket traffic must use same-origin `/api` paths from the frontend origin. The browser must not assume the backend port is reachable. Vite proxies all `/api` HTTP and WebSocket traffic to the backend listener.

Stable deployment defaults are centralized in:

```text
backend/include/schoolmanager/config/Constants.h
frontend/src/config/constants.ts
```

Deployment-specific overrides use environment variables instead of source edits. The backend uses `SM_HOST`, `SM_PORT`, `SM_MAX_UPLOAD_BYTES`, and `SM_MAX_UPLOAD_MEMORY_BYTES`; the frontend uses `SM_FRONTEND_HOST`, `SM_FRONTEND_PORT`, `SM_API_PROXY_HOST`, and `SM_API_PROXY_PORT`.

## Frontend State

Frontend state is split by responsibility:

- TanStack Query owns server data and network mutation state.
- Zustand owns local dirty-field tracking.
- React component state owns short-lived input drafts and upload progress.

Student names, student information values, and grade cells autosave on `blur`. The Global Settings view manages school-wide student information definitions. The student list and global settings are kept fresh through the school WebSocket room without requiring a page refresh. The UI exposes saved/saving and live/offline status.

Student detail headers display the student's `display_name` and `id`. The `folder_path` field remains storage metadata for backend/file-system bookkeeping and must not be used as a user-facing student label.

## API Contract

The OpenAPI contract is the source of truth for HTTP endpoints:

```text
backend/openapi/openapi.json
GET /api/openapi.json
```

When changing routes or payloads, update the OpenAPI file in the same change.

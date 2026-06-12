# Project Instructions

## Scope Discipline

- Keep backend and frontend separated.
- Keep backend domain, infra, and adapters separated.
- Do not move persistence logic into HTTP controllers.
- Do not move Drogon, SQLite, or file-system dependencies into the domain layer.
- Do not introduce Docker unless explicitly requested.
- Do not use C++ Modules.

## No Hardcoding

Do not scatter hardcoded strings, numbers, paths, route fragments, table names, JSON field names, or protocol constants through implementation files.

Preferred order:

1. Use configuration from environment variables when the value is deployment-specific.
2. Use a dedicated C++ constants header for stable internal constants.
3. Use local `constexpr` values only when the constant is private to a very small implementation block.

For backend-wide constants, add or use a header under:

```text
backend/include/schoolmanager/config/
```

Recommended default file:

```text
backend/include/schoolmanager/config/Constants.h
```

Examples of values that belong in constants/config:

- API route roots
- WebSocket route path and query parameter names
- SQLite PRAGMA values
- DB file names such as `school_index.db` and `data.db`
- folder names such as `students` and `uploads`
- upload status strings such as `pending` and `active`
- JSON field names used across files
- default host, port, data root, OpenAPI path, and DB pool size

If a value appears in more than one file, it must not be duplicated manually.

## C++ Style

- Use C++23 where it improves clarity or safety.
- Prefer RAII for resource ownership.
- Prefer `std::unique_ptr` for exclusive ownership.
- Prefer `std::shared_ptr` only for real shared lifetime, such as Drogon controller dependencies and connection pool handles.
- Prefer `std::weak_ptr` to avoid ownership cycles.
- Prefer `std::optional` for maybe-present values.
- Prefer `std::filesystem::path` for paths.
- Prefer `std::string_view` for non-owning string parameters when lifetime is clear.
- Prefer `constexpr`/`constinit` for compile-time constants.
- Prefer structured bindings and range loops when they improve readability.
- Avoid raw owning pointers.
- Avoid manual `new` and `delete`.
- Avoid global mutable state unless it is required by a framework registration mechanism and is isolated in the adapter layer.

## SQLite and File Storage

- `school_index.db` is for school-wide metadata only.
- Never query every student `data.db` to build the school list.
- Every student owns a separate folder and separate `data.db`.
- Always enable WAL and `busy_timeout` on SQLite connections.
- File uploads must use the tmp-write, DB-pending, atomic-rename, DB-active sequence.
- Do not store uploaded file bytes in SQLite.

## API and Realtime

- HTTP payload and route changes must be reflected in `backend/openapi/openapi.json`.
- Grade PATCH endpoints must update only provided fields.
- WebSocket broadcasts must include enough information for the frontend to avoid overwriting dirty local fields.
- Do not add overwrite warnings or conflict dialogs until that feature is explicitly requested.

## Frontend

- Keep TanStack Query responsible for server data and mutation state.
- Keep Zustand responsible for local dirty-field state.
- Autosave grade edits on blur.
- Do not require a manual save button for grade-cell edits.
- Do not overwrite a locally dirty field with incoming WebSocket data.
- Keep UI controls stable in size so editing/saving states do not shift layout.

## Documentation

- README is for user-facing setup and running instructions.
- Architecture details belong in `AGENTS/ARCH.md`.
- Engineering rules belong in `AGENTS/INSTRUCTION.md`.

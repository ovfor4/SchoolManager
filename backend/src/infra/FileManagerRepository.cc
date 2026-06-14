#include "schoolmanager/infra/FileManagerRepository.h"

#include "schoolmanager/config/Constants.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace schoolmanager::infra {

namespace {

void bindOptionalText(Statement& stmt, int index, const std::optional<std::string>& value)
{
    if (value) {
        stmt.bindText(index, *value);
    } else {
        stmt.bindNull(index);
    }
}

domain::FileEntry readFileEntry(Statement& stmt)
{
    return domain::FileEntry{
        .id = stmt.columnText(0),
        .context_type = stmt.columnText(1),
        .context_id = stmt.columnText(2),
        .parent_id = stmt.columnOptionalText(3),
        .kind = stmt.columnText(4),
        .name = stmt.columnText(5),
        .storage_name = stmt.columnText(6),
        .mime_type = stmt.columnText(7),
        .size_bytes = static_cast<std::uint64_t>(stmt.columnInt64(8)),
        .status = stmt.columnText(9),
        .trash_entry_id = stmt.columnOptionalText(10),
        .created_at = stmt.columnInt64(11),
        .updated_at = stmt.columnInt64(12),
        .trashed_at = stmt.columnOptionalInt64(13),
    };
}

domain::TrashEntry readTrashEntry(Statement& stmt)
{
    return domain::TrashEntry{
        .id = stmt.columnText(0),
        .context_type = stmt.columnText(1),
        .context_id = stmt.columnText(2),
        .root_entry_id = stmt.columnText(3),
        .original_parent_id = stmt.columnOptionalText(4),
        .root_name = stmt.columnText(5),
        .root_kind = stmt.columnText(6),
        .item_count = static_cast<std::uint64_t>(stmt.columnInt64(8)),
        .trashed_at = stmt.columnInt64(7),
    };
}

bool entryExistsUnlocked(SqliteConnection& db,
                         const domain::FileContext& context,
                         std::string_view entryId)
{
    auto stmt = db.prepare(R"SQL(
        SELECT 1
        FROM file_entries
        WHERE context_type = ? AND context_id = ? AND id = ?
        LIMIT 1;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, entryId);
    return stmt.step();
}

bool activeSiblingNameExistsUnlocked(SqliteConnection& db,
                                     const domain::FileContext& context,
                                     const std::optional<std::string>& parentId,
                                     std::string_view name,
                                     const std::optional<std::string>& excludedEntryId)
{
    const auto excludedClause = excludedEntryId ? " AND id <> ?" : "";
    std::ostringstream sql;
    sql << R"SQL(
        SELECT 1
        FROM file_entries
        WHERE context_type = ? AND context_id = ?
          AND status = ?
          AND name = ?
    )SQL";
    if (parentId) {
        sql << " AND parent_id = ?";
    } else {
        sql << " AND parent_id IS NULL";
    }
    sql << excludedClause << " LIMIT 1;";

    auto stmt = db.prepare(sql.str());
    int index = 1;
    stmt.bindText(index++, context.type);
    stmt.bindText(index++, context.id);
    stmt.bindText(index++, config::fileEntryStatusActive);
    stmt.bindText(index++, name);
    if (parentId) {
        stmt.bindText(index++, *parentId);
    }
    if (excludedEntryId) {
        stmt.bindText(index, *excludedEntryId);
    }
    return stmt.step();
}

std::string legacyDisplayName(std::string name)
{
    if (name.empty() || name == "." || name == "..") {
        return "uploaded_file";
    }
    std::replace(name.begin(), name.end(), '/', '_');
    std::replace(name.begin(), name.end(), '\\', '_');
    return name;
}

std::string nameWithCopySuffix(const std::string& name, int copyIndex)
{
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return name + " (" + std::to_string(copyIndex) + ")";
    }
    return name.substr(0, dot) + " (" + std::to_string(copyIndex) + ")" + name.substr(dot);
}

std::string availableLegacyName(SqliteConnection& db,
                                const domain::FileContext& context,
                                std::string name)
{
    if (!activeSiblingNameExistsUnlocked(db, context, std::nullopt, name, std::nullopt)) {
        return name;
    }

    for (int copyIndex = 2; copyIndex < 10000; ++copyIndex) {
        const auto candidate = nameWithCopySuffix(name, copyIndex);
        if (!activeSiblingNameExistsUnlocked(db, context, std::nullopt, candidate, std::nullopt)) {
            return candidate;
        }
    }
    throw std::runtime_error("failed to find available legacy file name");
}

void rollbackQuietly(SqliteConnection& db)
{
    try {
        db.executeUnlocked("ROLLBACK;");
    } catch (...) {
    }
}

bool isStudentUploadsContext(const domain::FileContext& context)
{
    return context.type == config::studentUploadsContextType && domain::isSafeId(context.id);
}

bool isGlobalTemplatesContext(const domain::FileContext& context)
{
    return context.type == config::globalTemplatesContextType &&
           context.id == config::defaultGlobalTemplatesContextId;
}

std::filesystem::path contextRootDir(const StoragePaths& paths, const domain::FileContext& context)
{
    if (context.type == config::studentUploadsContextType) {
        return paths.studentUploadsDir(context.id);
    }
    if (context.type == config::globalTemplatesContextType) {
        return paths.globalTemplateLibraryDir(context.id);
    }
    throw std::runtime_error("unsupported file context type");
}

}  // namespace

FileManagerRepository::FileManagerRepository(std::shared_ptr<LruSqlitePool> pool,
                                             StoragePaths paths)
    : pool_(std::move(pool)), paths_(std::move(paths))
{
}

void FileManagerRepository::initializeContext(const domain::FileContext& context)
{
    if (!isStudentUploadsContext(context) && !isGlobalTemplatesContext(context)) {
        throw std::runtime_error("unsupported file context type");
    }

    std::filesystem::create_directories(contextRootDir(paths_, context));
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    db->executeUnlocked(R"SQL(
        CREATE TABLE IF NOT EXISTS file_entries (
            id TEXT PRIMARY KEY,
            context_type TEXT NOT NULL,
            context_id TEXT NOT NULL,
            parent_id TEXT,
            kind TEXT NOT NULL CHECK(kind IN ('file', 'folder')),
            name TEXT NOT NULL,
            storage_name TEXT NOT NULL DEFAULT '',
            mime_type TEXT NOT NULL DEFAULT '',
            size_bytes INTEGER NOT NULL DEFAULT 0,
            status TEXT NOT NULL CHECK(status IN ('pending', 'active', 'trashed')),
            trash_entry_id TEXT,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            trashed_at INTEGER
        );
    )SQL");
    db->executeUnlocked(R"SQL(
        CREATE UNIQUE INDEX IF NOT EXISTS file_entries_active_sibling_name_idx
        ON file_entries(context_type, context_id, COALESCE(parent_id, ''), name)
        WHERE status = 'active';
    )SQL");
    db->executeUnlocked(R"SQL(
        CREATE INDEX IF NOT EXISTS file_entries_parent_idx
        ON file_entries(context_type, context_id, parent_id, status);
    )SQL");
    db->executeUnlocked(R"SQL(
        CREATE INDEX IF NOT EXISTS file_entries_trash_idx
        ON file_entries(context_type, context_id, trash_entry_id);
    )SQL");
    db->executeUnlocked(R"SQL(
        CREATE TABLE IF NOT EXISTS trash_entries (
            id TEXT PRIMARY KEY,
            context_type TEXT NOT NULL,
            context_id TEXT NOT NULL,
            root_entry_id TEXT NOT NULL,
            original_parent_id TEXT,
            root_name TEXT NOT NULL,
            root_kind TEXT NOT NULL CHECK(root_kind IN ('file', 'folder')),
            trashed_at INTEGER NOT NULL
        );
    )SQL");
    migrateLegacyStudentFilesUnlocked(*db, context);
}

std::vector<domain::FileEntry> FileManagerRepository::listEntries(
    const domain::FileContext& context,
    const std::optional<std::string>& parentId)
{
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();

    std::ostringstream sql;
    sql << R"SQL(
        SELECT id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
               size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at
        FROM file_entries
        WHERE context_type = ? AND context_id = ? AND status = ?
    )SQL";
    if (parentId) {
        sql << " AND parent_id = ?";
    } else {
        sql << " AND parent_id IS NULL";
    }
    sql << R"SQL(
        ORDER BY CASE kind WHEN 'folder' THEN 0 ELSE 1 END,
                 name COLLATE NOCASE ASC,
                 created_at DESC;
    )SQL";

    auto stmt = db->prepare(sql.str());
    int index = 1;
    stmt.bindText(index++, context.type);
    stmt.bindText(index++, context.id);
    stmt.bindText(index++, config::fileEntryStatusActive);
    if (parentId) {
        stmt.bindText(index, *parentId);
    }

    std::vector<domain::FileEntry> entries;
    while (stmt.step()) {
        entries.push_back(readFileEntry(stmt));
    }
    return entries;
}

std::optional<domain::FileEntry> FileManagerRepository::getEntry(
    const domain::FileContext& context,
    std::string_view entryId)
{
    if (!domain::isSafeId(entryId)) {
        return std::nullopt;
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
               size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at
        FROM file_entries
        WHERE context_type = ? AND context_id = ? AND id = ?;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, entryId);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return readFileEntry(stmt);
}

std::optional<domain::FileEntry> FileManagerRepository::getActiveFolder(
    const domain::FileContext& context,
    std::string_view entryId)
{
    if (!domain::isSafeId(entryId)) {
        return std::nullopt;
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
               size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at
        FROM file_entries
        WHERE context_type = ? AND context_id = ? AND id = ? AND kind = ? AND status = ?;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, entryId);
    stmt.bindText(4, config::fileEntryKindFolder);
    stmt.bindText(5, config::fileEntryStatusActive);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return readFileEntry(stmt);
}

std::vector<domain::FileEntry> FileManagerRepository::listSubtree(
    const domain::FileContext& context,
    std::string_view rootEntryId)
{
    if (!domain::isSafeId(rootEntryId)) {
        return {};
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        WITH RECURSIVE subtree AS (
            SELECT id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
                   size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at
            FROM file_entries
            WHERE context_type = ? AND context_id = ? AND id = ? AND status = ?
            UNION ALL
            SELECT child.id, child.context_type, child.context_id, child.parent_id, child.kind,
                   child.name, child.storage_name, child.mime_type, child.size_bytes, child.status,
                   child.trash_entry_id, child.created_at, child.updated_at, child.trashed_at
            FROM file_entries child
            JOIN subtree parent ON child.parent_id = parent.id
            WHERE child.context_type = ? AND child.context_id = ? AND child.status = ?
        )
        SELECT id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
               size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at
        FROM subtree;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, rootEntryId);
    stmt.bindText(4, config::fileEntryStatusActive);
    stmt.bindText(5, context.type);
    stmt.bindText(6, context.id);
    stmt.bindText(7, config::fileEntryStatusActive);

    std::vector<domain::FileEntry> entries;
    while (stmt.step()) {
        entries.push_back(readFileEntry(stmt));
    }
    return entries;
}

bool FileManagerRepository::activeSiblingNameExists(
    const domain::FileContext& context,
    const std::optional<std::string>& parentId,
    std::string_view name,
    const std::optional<std::string>& excludedEntryId)
{
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    return activeSiblingNameExistsUnlocked(*db, context, parentId, name, excludedEntryId);
}

domain::FileEntry FileManagerRepository::createFolder(const domain::FileContext& context,
                                                      const std::optional<std::string>& parentId,
                                                      std::string name)
{
    initializeContext(context);
    const auto now = domain::unixTimeMillis();
    domain::FileEntry entry{
        .id = domain::createId(),
        .context_type = context.type,
        .context_id = context.id,
        .parent_id = parentId,
        .kind = std::string(config::fileEntryKindFolder),
        .name = std::move(name),
        .storage_name = "",
        .mime_type = "",
        .size_bytes = 0,
        .status = std::string(config::fileEntryStatusActive),
        .trash_entry_id = std::nullopt,
        .created_at = now,
        .updated_at = now,
        .trashed_at = std::nullopt,
    };

    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        INSERT INTO file_entries
            (id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
             size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )SQL");
    stmt.bindText(1, entry.id);
    stmt.bindText(2, entry.context_type);
    stmt.bindText(3, entry.context_id);
    bindOptionalText(stmt, 4, entry.parent_id);
    stmt.bindText(5, entry.kind);
    stmt.bindText(6, entry.name);
    stmt.bindText(7, entry.storage_name);
    stmt.bindText(8, entry.mime_type);
    stmt.bindInt64(9, static_cast<std::int64_t>(entry.size_bytes));
    stmt.bindText(10, entry.status);
    bindOptionalText(stmt, 11, entry.trash_entry_id);
    stmt.bindInt64(12, entry.created_at);
    stmt.bindInt64(13, entry.updated_at);
    stmt.bindNull(14);
    stmt.executeDone();
    return entry;
}

domain::FileEntry FileManagerRepository::createPendingFile(
    const domain::FileContext& context,
    const std::optional<std::string>& parentId,
    std::string name,
    std::string storageName,
    std::string mimeType,
    std::uint64_t sizeBytes)
{
    initializeContext(context);
    const auto now = domain::unixTimeMillis();
    domain::FileEntry entry{
        .id = domain::createId(),
        .context_type = context.type,
        .context_id = context.id,
        .parent_id = parentId,
        .kind = std::string(config::fileEntryKindFile),
        .name = std::move(name),
        .storage_name = std::move(storageName),
        .mime_type = std::move(mimeType),
        .size_bytes = sizeBytes,
        .status = std::string(config::fileEntryStatusPending),
        .trash_entry_id = std::nullopt,
        .created_at = now,
        .updated_at = now,
        .trashed_at = std::nullopt,
    };

    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        INSERT INTO file_entries
            (id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
             size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )SQL");
    stmt.bindText(1, entry.id);
    stmt.bindText(2, entry.context_type);
    stmt.bindText(3, entry.context_id);
    bindOptionalText(stmt, 4, entry.parent_id);
    stmt.bindText(5, entry.kind);
    stmt.bindText(6, entry.name);
    stmt.bindText(7, entry.storage_name);
    stmt.bindText(8, entry.mime_type);
    stmt.bindInt64(9, static_cast<std::int64_t>(entry.size_bytes));
    stmt.bindText(10, entry.status);
    bindOptionalText(stmt, 11, entry.trash_entry_id);
    stmt.bindInt64(12, entry.created_at);
    stmt.bindInt64(13, entry.updated_at);
    stmt.bindNull(14);
    stmt.executeDone();
    return entry;
}

std::optional<domain::FileEntry> FileManagerRepository::activateFile(
    const domain::FileContext& context,
    std::string_view entryId)
{
    if (!domain::isSafeId(entryId)) {
        return std::nullopt;
    }
    initializeContext(context);
    const auto now = domain::unixTimeMillis();
    auto db = acquireContextDb(context);
    {
        auto guard = db->lock();
        auto stmt = db->prepare(R"SQL(
            UPDATE file_entries
            SET status = ?, updated_at = ?
            WHERE context_type = ? AND context_id = ? AND id = ? AND status = ?;
        )SQL");
        stmt.bindText(1, config::fileEntryStatusActive);
        stmt.bindInt64(2, now);
        stmt.bindText(3, context.type);
        stmt.bindText(4, context.id);
        stmt.bindText(5, entryId);
        stmt.bindText(6, config::fileEntryStatusPending);
        stmt.executeDone();
        if (sqlite3_changes(db->raw()) == 0) {
            return std::nullopt;
        }
    }
    return getEntry(context, entryId);
}

void FileManagerRepository::deleteEntryRecord(const domain::FileContext& context,
                                              std::string_view entryId)
{
    if (!domain::isSafeId(entryId)) {
        return;
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        DELETE FROM file_entries
        WHERE context_type = ? AND context_id = ? AND id = ?;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, entryId);
    stmt.executeDone();
}

std::optional<domain::FileEntry> FileManagerRepository::renameEntry(
    const domain::FileContext& context,
    std::string_view entryId,
    std::string name)
{
    if (!domain::isSafeId(entryId)) {
        return std::nullopt;
    }
    initializeContext(context);
    const auto now = domain::unixTimeMillis();
    auto db = acquireContextDb(context);
    {
        auto guard = db->lock();
        auto stmt = db->prepare(R"SQL(
            UPDATE file_entries
            SET name = ?, updated_at = ?
            WHERE context_type = ? AND context_id = ? AND id = ? AND status = ?;
        )SQL");
        stmt.bindText(1, name);
        stmt.bindInt64(2, now);
        stmt.bindText(3, context.type);
        stmt.bindText(4, context.id);
        stmt.bindText(5, entryId);
        stmt.bindText(6, config::fileEntryStatusActive);
        stmt.executeDone();
        if (sqlite3_changes(db->raw()) == 0) {
            return std::nullopt;
        }
    }
    return getEntry(context, entryId);
}

std::vector<domain::TrashEntry> FileManagerRepository::listTrash(
    const domain::FileContext& context)
{
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT t.id, t.context_type, t.context_id, t.root_entry_id, t.original_parent_id,
               t.root_name, t.root_kind, t.trashed_at, COUNT(f.id) AS item_count
        FROM trash_entries t
        LEFT JOIN file_entries f
               ON f.context_type = t.context_type
              AND f.context_id = t.context_id
              AND f.trash_entry_id = t.id
        WHERE t.context_type = ? AND t.context_id = ?
        GROUP BY t.id, t.context_type, t.context_id, t.root_entry_id, t.original_parent_id,
                 t.root_name, t.root_kind, t.trashed_at
        ORDER BY t.trashed_at DESC;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);

    std::vector<domain::TrashEntry> entries;
    while (stmt.step()) {
        entries.push_back(readTrashEntry(stmt));
    }
    return entries;
}

std::optional<domain::TrashEntry> FileManagerRepository::getTrashEntry(
    const domain::FileContext& context,
    std::string_view trashEntryId)
{
    if (!domain::isSafeId(trashEntryId)) {
        return std::nullopt;
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT t.id, t.context_type, t.context_id, t.root_entry_id, t.original_parent_id,
               t.root_name, t.root_kind, t.trashed_at, COUNT(f.id) AS item_count
        FROM trash_entries t
        LEFT JOIN file_entries f
               ON f.context_type = t.context_type
              AND f.context_id = t.context_id
              AND f.trash_entry_id = t.id
        WHERE t.context_type = ? AND t.context_id = ? AND t.id = ?
        GROUP BY t.id, t.context_type, t.context_id, t.root_entry_id, t.original_parent_id,
                 t.root_name, t.root_kind, t.trashed_at;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, trashEntryId);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return readTrashEntry(stmt);
}

std::vector<domain::FileEntry> FileManagerRepository::listTrashEntries(
    const domain::FileContext& context,
    std::string_view trashEntryId)
{
    if (!domain::isSafeId(trashEntryId)) {
        return {};
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    auto stmt = db->prepare(R"SQL(
        SELECT id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
               size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at
        FROM file_entries
        WHERE context_type = ? AND context_id = ? AND trash_entry_id = ? AND status = ?
        ORDER BY created_at ASC;
    )SQL");
    stmt.bindText(1, context.type);
    stmt.bindText(2, context.id);
    stmt.bindText(3, trashEntryId);
    stmt.bindText(4, config::fileEntryStatusTrashed);

    std::vector<domain::FileEntry> entries;
    while (stmt.step()) {
        entries.push_back(readFileEntry(stmt));
    }
    return entries;
}

void FileManagerRepository::markSubtreeTrashed(const domain::FileContext& context,
                                               const domain::TrashEntry& trashEntry,
                                               std::int64_t trashedAt)
{
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    db->executeUnlocked("BEGIN IMMEDIATE;");
    try {
        auto trashStmt = db->prepare(R"SQL(
            INSERT INTO trash_entries
                (id, context_type, context_id, root_entry_id, original_parent_id,
                 root_name, root_kind, trashed_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?);
        )SQL");
        trashStmt.bindText(1, trashEntry.id);
        trashStmt.bindText(2, context.type);
        trashStmt.bindText(3, context.id);
        trashStmt.bindText(4, trashEntry.root_entry_id);
        bindOptionalText(trashStmt, 5, trashEntry.original_parent_id);
        trashStmt.bindText(6, trashEntry.root_name);
        trashStmt.bindText(7, trashEntry.root_kind);
        trashStmt.bindInt64(8, trashedAt);
        trashStmt.executeDone();

        auto updateStmt = db->prepare(R"SQL(
            WITH RECURSIVE subtree AS (
                SELECT id
                FROM file_entries
                WHERE context_type = ? AND context_id = ? AND id = ? AND status = ?
                UNION ALL
                SELECT child.id
                FROM file_entries child
                JOIN subtree parent ON child.parent_id = parent.id
                WHERE child.context_type = ? AND child.context_id = ? AND child.status = ?
            )
            UPDATE file_entries
            SET status = ?,
                trash_entry_id = ?,
                trashed_at = ?,
                updated_at = ?
            WHERE context_type = ? AND context_id = ? AND id IN (SELECT id FROM subtree);
        )SQL");
        updateStmt.bindText(1, context.type);
        updateStmt.bindText(2, context.id);
        updateStmt.bindText(3, trashEntry.root_entry_id);
        updateStmt.bindText(4, config::fileEntryStatusActive);
        updateStmt.bindText(5, context.type);
        updateStmt.bindText(6, context.id);
        updateStmt.bindText(7, config::fileEntryStatusActive);
        updateStmt.bindText(8, config::fileEntryStatusTrashed);
        updateStmt.bindText(9, trashEntry.id);
        updateStmt.bindInt64(10, trashedAt);
        updateStmt.bindInt64(11, trashedAt);
        updateStmt.bindText(12, context.type);
        updateStmt.bindText(13, context.id);
        updateStmt.executeDone();

        db->executeUnlocked("COMMIT;");
    } catch (...) {
        rollbackQuietly(*db);
        throw;
    }
}

void FileManagerRepository::restoreTrashEntry(const domain::FileContext& context,
                                              std::string_view trashEntryId,
                                              std::int64_t restoredAt)
{
    if (!domain::isSafeId(trashEntryId)) {
        return;
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    db->executeUnlocked("BEGIN IMMEDIATE;");
    try {
        auto updateStmt = db->prepare(R"SQL(
            UPDATE file_entries
            SET status = ?,
                trash_entry_id = NULL,
                trashed_at = NULL,
                updated_at = ?
            WHERE context_type = ? AND context_id = ? AND trash_entry_id = ? AND status = ?;
        )SQL");
        updateStmt.bindText(1, config::fileEntryStatusActive);
        updateStmt.bindInt64(2, restoredAt);
        updateStmt.bindText(3, context.type);
        updateStmt.bindText(4, context.id);
        updateStmt.bindText(5, trashEntryId);
        updateStmt.bindText(6, config::fileEntryStatusTrashed);
        updateStmt.executeDone();

        auto deleteStmt = db->prepare(R"SQL(
            DELETE FROM trash_entries
            WHERE context_type = ? AND context_id = ? AND id = ?;
        )SQL");
        deleteStmt.bindText(1, context.type);
        deleteStmt.bindText(2, context.id);
        deleteStmt.bindText(3, trashEntryId);
        deleteStmt.executeDone();

        db->executeUnlocked("COMMIT;");
    } catch (...) {
        rollbackQuietly(*db);
        throw;
    }
}

void FileManagerRepository::permanentlyDeleteTrashEntry(const domain::FileContext& context,
                                                        std::string_view trashEntryId)
{
    if (!domain::isSafeId(trashEntryId)) {
        return;
    }
    initializeContext(context);
    auto db = acquireContextDb(context);
    auto guard = db->lock();
    db->executeUnlocked("BEGIN IMMEDIATE;");
    try {
        auto entriesStmt = db->prepare(R"SQL(
            DELETE FROM file_entries
            WHERE context_type = ? AND context_id = ? AND trash_entry_id = ? AND status = ?;
        )SQL");
        entriesStmt.bindText(1, context.type);
        entriesStmt.bindText(2, context.id);
        entriesStmt.bindText(3, trashEntryId);
        entriesStmt.bindText(4, config::fileEntryStatusTrashed);
        entriesStmt.executeDone();

        auto trashStmt = db->prepare(R"SQL(
            DELETE FROM trash_entries
            WHERE context_type = ? AND context_id = ? AND id = ?;
        )SQL");
        trashStmt.bindText(1, context.type);
        trashStmt.bindText(2, context.id);
        trashStmt.bindText(3, trashEntryId);
        trashStmt.executeDone();

        db->executeUnlocked("COMMIT;");
    } catch (...) {
        rollbackQuietly(*db);
        throw;
    }
}

std::shared_ptr<SqliteConnection> FileManagerRepository::acquireContextDb(
    const domain::FileContext& context)
{
    if (isStudentUploadsContext(context)) {
        return pool_->acquire(paths_.studentDataDb(context.id));
    }
    if (isGlobalTemplatesContext(context)) {
        return pool_->acquire(paths_.schoolIndexDb());
    }
    throw std::runtime_error("unsupported file context type");
}

void FileManagerRepository::migrateLegacyStudentFilesUnlocked(SqliteConnection& db,
                                                              const domain::FileContext& context)
{
    if (context.type != config::studentUploadsContextType) {
        return;
    }

    auto tableStmt = db.prepare(R"SQL(
        SELECT 1
        FROM sqlite_master
        WHERE type = 'table' AND name = 'files'
        LIMIT 1;
    )SQL");
    if (!tableStmt.step()) {
        return;
    }

    struct LegacyFile {
        std::string id;
        std::string original_name;
        std::string stored_name;
        std::string mime_type;
        std::string status;
        std::uint64_t size_bytes{};
        std::int64_t created_at{};
        std::int64_t updated_at{};
    };

    std::vector<LegacyFile> legacyFiles;
    auto legacyStmt = db.prepare(R"SQL(
        SELECT id, original_name, stored_name, mime_type, status, size_bytes, created_at, updated_at
        FROM files
        ORDER BY created_at ASC, id ASC;
    )SQL");
    while (legacyStmt.step()) {
        legacyFiles.push_back(LegacyFile{
            .id = legacyStmt.columnText(0),
            .original_name = legacyStmt.columnText(1),
            .stored_name = legacyStmt.columnText(2),
            .mime_type = legacyStmt.columnText(3),
            .status = legacyStmt.columnText(4),
            .size_bytes = static_cast<std::uint64_t>(legacyStmt.columnInt64(5)),
            .created_at = legacyStmt.columnInt64(6),
            .updated_at = legacyStmt.columnInt64(7),
        });
    }

    for (const auto& legacy : legacyFiles) {
        if (!domain::isSafeId(legacy.id) || entryExistsUnlocked(db, context, legacy.id)) {
            continue;
        }
        if (legacy.status != config::fileEntryStatusPending &&
            legacy.status != config::fileEntryStatusActive) {
            continue;
        }

        auto displayName = legacyDisplayName(legacy.original_name);
        if (legacy.status == config::fileEntryStatusActive) {
            displayName = availableLegacyName(db, context, std::move(displayName));
        }

        auto insertStmt = db.prepare(R"SQL(
            INSERT INTO file_entries
                (id, context_type, context_id, parent_id, kind, name, storage_name, mime_type,
                 size_bytes, status, trash_entry_id, created_at, updated_at, trashed_at)
            VALUES (?, ?, ?, NULL, ?, ?, ?, ?, ?, ?, NULL, ?, ?, NULL);
        )SQL");
        insertStmt.bindText(1, legacy.id);
        insertStmt.bindText(2, context.type);
        insertStmt.bindText(3, context.id);
        insertStmt.bindText(4, config::fileEntryKindFile);
        insertStmt.bindText(5, displayName);
        insertStmt.bindText(6, legacy.stored_name);
        insertStmt.bindText(7, legacy.mime_type);
        insertStmt.bindInt64(8, static_cast<std::int64_t>(legacy.size_bytes));
        insertStmt.bindText(9, legacy.status);
        insertStmt.bindInt64(10, legacy.created_at);
        insertStmt.bindInt64(11, legacy.updated_at);
        insertStmt.executeDone();
    }
}

}  // namespace schoolmanager::infra

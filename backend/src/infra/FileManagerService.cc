#include "schoolmanager/infra/FileManagerService.h"

#include "schoolmanager/config/Constants.h"
#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <utility>

namespace schoolmanager::infra {

namespace {

struct FileMove {
    std::filesystem::path source;
    std::filesystem::path destination;
};

std::string trimAscii(std::string value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    if (begin == end) {
        return {};
    }
    return value.substr(begin, end - begin);
}

std::string fileNameOnly(std::string value)
{
    const auto slash = value.find_last_of("/\\");
    if (slash != std::string::npos) {
        value = value.substr(slash + 1);
    }
    return value.empty() ? "uploaded_file" : value;
}

std::string validateEntryName(std::string name)
{
    name = trimAscii(std::move(name));
    if (name.empty()) {
        throw FileManagerError(FileManagerErrorCode::BadRequest, "file name is required");
    }
    if (name == "." || name == "..") {
        throw FileManagerError(FileManagerErrorCode::BadRequest, "file name is invalid");
    }
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        throw FileManagerError(FileManagerErrorCode::BadRequest, "file name cannot contain path separators");
    }
    if (name.size() > 255) {
        throw FileManagerError(FileManagerErrorCode::BadRequest, "file name is too long");
    }
    return name;
}

std::string uploadDisplayName(const FileUploadPayload& payload)
{
    return validateEntryName(fileNameOnly(payload.file_name));
}

std::filesystem::path relativeFolderPathFor(
    const domain::FileEntry& file,
    const domain::FileEntry& root,
    const std::map<std::string, domain::FileEntry>& entriesById)
{
    if (file.id == root.id) {
        return {};
    }

    std::vector<std::string> parts;
    auto parentId = file.parent_id;
    while (parentId && *parentId != root.id) {
        const auto parent = entriesById.find(*parentId);
        if (parent == entriesById.end()) {
            throw FileManagerError(FileManagerErrorCode::Conflict,
                                   "file tree metadata is incomplete");
        }
        parts.push_back(parent->second.name);
        parentId = parent->second.parent_id;
    }
    if (!parentId) {
        throw FileManagerError(FileManagerErrorCode::Conflict, "file tree metadata is incomplete");
    }

    std::filesystem::path relative;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        relative /= *it;
    }
    return relative;
}

std::map<std::string, domain::FileEntry> entriesById(
    const std::vector<domain::FileEntry>& entries)
{
    std::map<std::string, domain::FileEntry> byId;
    for (const auto& entry : entries) {
        byId.emplace(entry.id, entry);
    }
    return byId;
}

const domain::FileEntry& rootEntryFor(const domain::TrashEntry& trashEntry,
                                      const std::vector<domain::FileEntry>& entries)
{
    const auto root = std::find_if(entries.begin(), entries.end(), [&](const domain::FileEntry& entry) {
        return entry.id == trashEntry.root_entry_id;
    });
    if (root == entries.end()) {
        throw FileManagerError(FileManagerErrorCode::Conflict, "trash entry metadata is incomplete");
    }
    return *root;
}

std::vector<FileMove> trashMovesFor(const FileStorage& storage,
                                    const domain::FileContext& context,
                                    const domain::TrashEntry& trashEntry,
                                    const std::vector<domain::FileEntry>& entries)
{
    const auto& root = rootEntryFor(trashEntry, entries);
    const auto byId = entriesById(entries);
    const auto trashDir = storage.trashEntryDir(context, trashEntry.id, trashEntry.root_name);

    std::vector<FileMove> moves;
    for (const auto& entry : entries) {
        if (entry.kind != config::fileEntryKindFile) {
            continue;
        }
        moves.push_back(FileMove{
            .source = storage.activeFilePath(context, entry.storage_name),
            .destination = trashDir / relativeFolderPathFor(entry, root, byId) / entry.storage_name,
        });
    }
    return moves;
}

std::vector<FileMove> restoreMovesFor(const FileStorage& storage,
                                      const domain::FileContext& context,
                                      const domain::TrashEntry& trashEntry,
                                      const std::vector<domain::FileEntry>& entries)
{
    const auto& root = rootEntryFor(trashEntry, entries);
    const auto byId = entriesById(entries);
    const auto trashDir = storage.trashEntryDir(context, trashEntry.id, trashEntry.root_name);

    std::vector<FileMove> moves;
    for (const auto& entry : entries) {
        if (entry.kind != config::fileEntryKindFile) {
            continue;
        }
        moves.push_back(FileMove{
            .source = trashDir / relativeFolderPathFor(entry, root, byId) / entry.storage_name,
            .destination = storage.activeFilePath(context, entry.storage_name),
        });
    }
    return moves;
}

void ensureMoveSourcesExist(const std::vector<FileMove>& moves)
{
    for (const auto& move : moves) {
        if (!std::filesystem::exists(move.source)) {
            throw FileManagerError(FileManagerErrorCode::NotFound, "file content missing");
        }
    }
}

void ensureMoveDestinationsAvailable(const std::vector<FileMove>& moves)
{
    for (const auto& move : moves) {
        if (std::filesystem::exists(move.destination)) {
            throw FileManagerError(FileManagerErrorCode::Conflict,
                                   "file content destination already exists");
        }
    }
}

void moveWithRollback(const FileStorage& storage, const std::vector<FileMove>& moves)
{
    std::vector<FileMove> moved;
    try {
        for (const auto& move : moves) {
            storage.moveFile(move.source, move.destination);
            moved.push_back(move);
        }
    } catch (...) {
        for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
            try {
                storage.moveFile(it->destination, it->source);
            } catch (...) {
            }
        }
        throw;
    }
}

void rollbackMoves(const FileStorage& storage, const std::vector<FileMove>& moves)
{
    for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
        try {
            if (std::filesystem::exists(it->destination)) {
                storage.moveFile(it->destination, it->source);
            }
        } catch (...) {
        }
    }
}

}  // namespace

FileManagerError::FileManagerError(FileManagerErrorCode code, const std::string& message)
    : std::runtime_error(message), code_(code)
{
}

FileManagerErrorCode FileManagerError::code() const noexcept
{
    return code_;
}

FileManagerService::FileManagerService(std::shared_ptr<FileManagerRepository> repository,
                                       FileStorage storage,
                                       std::shared_ptr<SchoolIndexRepository> schoolIndex)
    : repository_(std::move(repository)),
      storage_(std::move(storage)),
      school_index_(std::move(schoolIndex))
{
}

std::vector<domain::FileEntry> FileManagerService::listEntries(
    const domain::FileContext& context,
    const std::optional<std::string>& parentId)
{
    ensureContext(context);
    ensureParent(context, parentId);
    return repository_->listEntries(context, parentId);
}

domain::FileEntry FileManagerService::uploadFile(const domain::FileContext& context,
                                                 const std::optional<std::string>& parentId,
                                                 const FileUploadPayload& payload)
{
    ensureContext(context);
    ensureParent(context, parentId);
    const auto name = uploadDisplayName(payload);
    ensureAvailableName(context, parentId, name);

    storage_.ensureRoot(context);
    const auto storageName = domain::createId() + "_" + sanitizeFileName(name);
    const auto tmpPath = storage_.tempFilePath(context, storageName);
    const auto finalPath = storage_.activeFilePath(context, storageName);

    try {
        if (!payload.write_temp_file) {
            throw std::runtime_error("upload writer is required");
        }
        payload.write_temp_file(tmpPath);
    } catch (...) {
        storage_.removeFileIfExists(tmpPath);
        throw;
    }

    const auto mimeType =
        payload.mime_type.empty() ? std::string("application/octet-stream") : payload.mime_type;
    auto pending = repository_->createPendingFile(
        context, parentId, name, storageName, mimeType, payload.size_bytes);

    try {
        storage_.moveFile(tmpPath, finalPath);
    } catch (...) {
        repository_->deleteEntryRecord(context, pending.id);
        storage_.removeFileIfExists(tmpPath);
        throw;
    }

    try {
        const auto active = repository_->activateFile(context, pending.id);
        if (!active) {
            throw std::runtime_error("failed to activate uploaded file");
        }
        touchContext(context, active->updated_at);
        return *active;
    } catch (...) {
        repository_->deleteEntryRecord(context, pending.id);
        storage_.removeFileIfExists(finalPath);
        throw;
    }
}

domain::FileEntry FileManagerService::createFolder(const domain::FileContext& context,
                                                   const std::optional<std::string>& parentId,
                                                   std::string name)
{
    ensureContext(context);
    ensureParent(context, parentId);
    name = validateEntryName(std::move(name));
    ensureAvailableName(context, parentId, name);

    auto folder = repository_->createFolder(context, parentId, std::move(name));
    touchContext(context, folder.updated_at);
    return folder;
}

domain::FileEntry FileManagerService::renameEntry(const domain::FileContext& context,
                                                  std::string_view entryId,
                                                  std::string name)
{
    ensureContext(context);
    name = validateEntryName(std::move(name));

    const auto entry = repository_->getEntry(context, entryId);
    if (!entry || entry->status != config::fileEntryStatusActive) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "file entry not found");
    }
    ensureAvailableName(context, entry->parent_id, name, entry->id);

    auto updated = repository_->renameEntry(context, entryId, std::move(name));
    if (!updated) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "file entry not found");
    }
    touchContext(context, updated->updated_at);
    return *updated;
}

void FileManagerService::moveEntryToTrash(const domain::FileContext& context,
                                          std::string_view entryId)
{
    ensureContext(context);
    const auto entry = repository_->getEntry(context, entryId);
    if (!entry || entry->status != config::fileEntryStatusActive) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "file entry not found");
    }

    const auto subtree = repository_->listSubtree(context, entryId);
    if (subtree.empty()) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "file entry not found");
    }

    const auto now = domain::unixTimeMillis();
    domain::TrashEntry trashEntry{
        .id = domain::createId(),
        .context_type = context.type,
        .context_id = context.id,
        .root_entry_id = entry->id,
        .original_parent_id = entry->parent_id,
        .root_name = entry->name,
        .root_kind = entry->kind,
        .item_count = subtree.size(),
        .trashed_at = now,
    };

    const auto moves = trashMovesFor(storage_, context, trashEntry, subtree);
    ensureMoveSourcesExist(moves);
    ensureMoveDestinationsAvailable(moves);
    moveWithRollback(storage_, moves);

    try {
        repository_->markSubtreeTrashed(context, trashEntry, now);
        touchContext(context, now);
    } catch (...) {
        rollbackMoves(storage_, moves);
        try {
            storage_.removeTreeIfExists(
                storage_.trashEntryDir(context, trashEntry.id, trashEntry.root_name));
        } catch (...) {
        }
        throw;
    }
}

FileDownload FileManagerService::downloadFile(const domain::FileContext& context,
                                              std::string_view entryId)
{
    ensureContext(context);
    const auto entry = repository_->getEntry(context, entryId);
    if (!entry || entry->status != config::fileEntryStatusActive ||
        entry->kind != config::fileEntryKindFile) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "file not found");
    }

    const auto path = storage_.activeFilePath(context, entry->storage_name);
    if (!std::filesystem::exists(path)) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "file content missing");
    }
    return FileDownload{.entry = *entry, .path = path};
}

std::vector<domain::TrashEntry> FileManagerService::listTrash(
    const domain::FileContext& context)
{
    ensureContext(context);
    return repository_->listTrash(context);
}

void FileManagerService::restoreTrashEntry(const domain::FileContext& context,
                                           std::string_view trashEntryId)
{
    ensureContext(context);
    const auto trashEntry = repository_->getTrashEntry(context, trashEntryId);
    if (!trashEntry) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "trash entry not found");
    }

    if (trashEntry->original_parent_id &&
        !repository_->getActiveFolder(context, *trashEntry->original_parent_id)) {
        throw FileManagerError(FileManagerErrorCode::Conflict,
                               "original parent folder is no longer available");
    }
    ensureAvailableName(context, trashEntry->original_parent_id, trashEntry->root_name);

    const auto entries = repository_->listTrashEntries(context, trashEntryId);
    if (entries.empty()) {
        throw FileManagerError(FileManagerErrorCode::Conflict, "trash entry metadata is incomplete");
    }

    const auto moves = restoreMovesFor(storage_, context, *trashEntry, entries);
    ensureMoveSourcesExist(moves);
    ensureMoveDestinationsAvailable(moves);
    moveWithRollback(storage_, moves);

    const auto now = domain::unixTimeMillis();
    try {
        repository_->restoreTrashEntry(context, trashEntryId, now);
        touchContext(context, now);
    } catch (...) {
        rollbackMoves(storage_, moves);
        throw;
    }

    try {
        storage_.removeTreeIfExists(
            storage_.trashEntryDir(context, trashEntry->id, trashEntry->root_name));
    } catch (...) {
    }
}

void FileManagerService::permanentlyDeleteTrashEntry(const domain::FileContext& context,
                                                     std::string_view trashEntryId)
{
    ensureContext(context);
    const auto trashEntry = repository_->getTrashEntry(context, trashEntryId);
    if (!trashEntry) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "trash entry not found");
    }

    storage_.removeTreeIfExists(
        storage_.trashEntryDir(context, trashEntry->id, trashEntry->root_name));
    const auto now = domain::unixTimeMillis();
    repository_->permanentlyDeleteTrashEntry(context, trashEntryId);
    touchContext(context, now);
}

void FileManagerService::ensureContext(const domain::FileContext& context)
{
    if (context.type != config::studentUploadsContextType) {
        throw FileManagerError(FileManagerErrorCode::BadRequest, "unsupported file context type");
    }
    if (!domain::isSafeId(context.id)) {
        throw FileManagerError(FileManagerErrorCode::BadRequest, "invalid file context id");
    }
    if (!school_index_->getStudent(context.id)) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "student not found");
    }
    storage_.ensureRoot(context);
    repository_->initializeContext(context);
}

void FileManagerService::ensureParent(const domain::FileContext& context,
                                      const std::optional<std::string>& parentId)
{
    if (!parentId) {
        return;
    }
    if (!domain::isSafeId(*parentId) || !repository_->getActiveFolder(context, *parentId)) {
        throw FileManagerError(FileManagerErrorCode::NotFound, "parent folder not found");
    }
}

void FileManagerService::ensureAvailableName(
    const domain::FileContext& context,
    const std::optional<std::string>& parentId,
    std::string_view name,
    const std::optional<std::string>& excludedEntryId)
{
    if (repository_->activeSiblingNameExists(context, parentId, name, excludedEntryId)) {
        throw FileManagerError(FileManagerErrorCode::Conflict,
                               "an entry with that name already exists");
    }
}

void FileManagerService::touchContext(const domain::FileContext& context, std::int64_t updatedAt)
{
    if (context.type == config::studentUploadsContextType) {
        school_index_->touchStudent(context.id, updatedAt);
    }
}

}  // namespace schoolmanager::infra

#include "schoolmanager/adapters/FileManagerController.h"

#include "schoolmanager/adapters/HttpJson.h"
#include "schoolmanager/adapters/JsonHelpers.h"
#include "schoolmanager/config/Constants.h"

#include <drogon/MultiPart.h>

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace schoolmanager::adapters {

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

domain::FileContext fileContext(std::string contextType, std::string contextId)
{
    return domain::FileContext{
        .type = std::move(contextType),
        .id = std::move(contextId),
    };
}

}  // namespace

FileManagerController::FileManagerController(
    std::shared_ptr<infra::FileManagerService> fileManager,
    std::shared_ptr<RealtimePublisher> realtime)
    : file_manager_(std::move(fileManager)), realtime_(std::move(realtime))
{
}

void FileManagerController::listEntries(const drogon::HttpRequestPtr& request,
                                        Callback&& callback,
                                        std::string contextType,
                                        std::string contextId)
{
    try {
        const auto entries =
            file_manager_->listEntries(fileContext(std::move(contextType), std::move(contextId)),
                                       optionalNonEmpty(request->getParameter("parent_id")));

        Json::Value body(Json::objectValue);
        body["entries"] = toJsonArray(entries);
        callback(jsonResponse(std::move(body)));
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void FileManagerController::uploadFile(const drogon::HttpRequestPtr& request,
                                       Callback&& callback,
                                       std::string contextType,
                                       std::string contextId)
{
    try {
        drogon::MultiPartParser parser;
        if (parser.parse(request) != 0 || parser.getFiles().empty()) {
            callback(errorResponse("multipart file field is required", drogon::k400BadRequest));
            return;
        }

        const auto& upload = parser.getFiles().front();
        auto context = fileContext(std::move(contextType), std::move(contextId));
        const auto entry = file_manager_->uploadFile(
            context,
            optionalNonEmpty(request->getParameter("parent_id")),
            infra::FileUploadPayload{
                .file_name = upload.getFileName().empty() ? "uploaded_file" : upload.getFileName(),
                .mime_type = std::string(config::octetStreamMimeType),
                .size_bytes = upload.fileLength(),
                .write_temp_file =
                    [&upload](const std::filesystem::path& path) {
                        if (upload.saveAs(path.string()) != 0) {
                            throw std::runtime_error("failed to write temporary upload file");
                        }
                    },
            });

        Json::Value body(Json::objectValue);
        body["entry"] = toJson(entry);
        callback(jsonResponse(std::move(body), drogon::k201Created));

        realtime_->fileManagerChanged(context, config::fileManagerEntryCreatedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void FileManagerController::createFolder(const drogon::HttpRequestPtr& request,
                                         Callback&& callback,
                                         std::string contextType,
                                         std::string contextId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("name")) {
            callback(errorResponse("name is required", drogon::k400BadRequest));
            return;
        }

        auto context = fileContext(std::move(contextType), std::move(contextId));
        const auto folder = file_manager_->createFolder(
            context,
            json->isMember("parent_id") ? optionalNonEmpty(scalarToString((*json)["parent_id"]))
                                        : std::nullopt,
            scalarToString((*json)["name"]));

        Json::Value body(Json::objectValue);
        body["entry"] = toJson(folder);
        callback(jsonResponse(std::move(body), drogon::k201Created));

        realtime_->fileManagerChanged(context, config::fileManagerEntryCreatedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void FileManagerController::renameEntry(const drogon::HttpRequestPtr& request,
                                        Callback&& callback,
                                        std::string contextType,
                                        std::string contextId,
                                        std::string entryId)
{
    try {
        const auto json = request->getJsonObject();
        if (!json || !json->isObject() || !json->isMember("name")) {
            callback(errorResponse("name is required", drogon::k400BadRequest));
            return;
        }

        auto context = fileContext(std::move(contextType), std::move(contextId));
        const auto entry =
            file_manager_->renameEntry(context, entryId, scalarToString((*json)["name"]));

        Json::Value body(Json::objectValue);
        body["entry"] = toJson(entry);
        callback(jsonResponse(std::move(body)));

        realtime_->fileManagerChanged(context, config::fileManagerEntryUpdatedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void FileManagerController::moveEntryToTrash(const drogon::HttpRequestPtr&,
                                             Callback&& callback,
                                             std::string contextType,
                                             std::string contextId,
                                             std::string entryId)
{
    try {
        auto context = fileContext(std::move(contextType), std::move(contextId));
        file_manager_->moveEntryToTrash(context, entryId);

        Json::Value body(Json::objectValue);
        body["trashed"] = true;
        callback(jsonResponse(std::move(body)));

        realtime_->fileManagerChanged(context, config::fileManagerEntryTrashedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void FileManagerController::downloadEntry(const drogon::HttpRequestPtr&,
                                          Callback&& callback,
                                          std::string contextType,
                                          std::string contextId,
                                          std::string entryId)
{
    try {
        const auto download =
            file_manager_->downloadFile(fileContext(std::move(contextType), std::move(contextId)),
                                        entryId);
        callback(drogon::HttpResponse::newFileResponse(download.path.string(), download.entry.name));
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void FileManagerController::listTrash(const drogon::HttpRequestPtr&,
                                      Callback&& callback,
                                      std::string contextType,
                                      std::string contextId)
{
    try {
        const auto trash =
            file_manager_->listTrash(fileContext(std::move(contextType), std::move(contextId)));

        Json::Value body(Json::objectValue);
        body["trash"] = toJsonArray(trash);
        callback(jsonResponse(std::move(body)));
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void FileManagerController::restoreTrashEntry(const drogon::HttpRequestPtr&,
                                              Callback&& callback,
                                              std::string contextType,
                                              std::string contextId,
                                              std::string trashEntryId)
{
    try {
        auto context = fileContext(std::move(contextType), std::move(contextId));
        file_manager_->restoreTrashEntry(context, trashEntryId);

        Json::Value body(Json::objectValue);
        body["restored"] = true;
        callback(jsonResponse(std::move(body)));

        realtime_->fileManagerChanged(context, config::fileManagerTrashRestoredAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

void FileManagerController::permanentlyDeleteTrashEntry(const drogon::HttpRequestPtr&,
                                                        Callback&& callback,
                                                        std::string contextType,
                                                        std::string contextId,
                                                        std::string trashEntryId)
{
    try {
        auto context = fileContext(std::move(contextType), std::move(contextId));
        file_manager_->permanentlyDeleteTrashEntry(context, trashEntryId);

        Json::Value body(Json::objectValue);
        body["deleted"] = true;
        callback(jsonResponse(std::move(body)));

        realtime_->fileManagerChanged(context, config::fileManagerTrashDeletedAction);
    } catch (const infra::FileManagerError& e) {
        callback(errorResponse(e.what(), statusForFileManagerError(e.code())));
    } catch (const std::exception& e) {
        callback(errorResponse(e.what(), drogon::k400BadRequest));
    }
}

}  // namespace schoolmanager::adapters

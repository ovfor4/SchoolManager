#pragma once

#include "schoolmanager/adapters/RealtimePublisher.h"
#include "schoolmanager/infra/FileManagerService.h"

#include <drogon/HttpController.h>

#include <functional>
#include <memory>
#include <string>

namespace schoolmanager::adapters {

class FileManagerController : public drogon::HttpController<FileManagerController, false> {
  public:
    FileManagerController(std::shared_ptr<infra::FileManagerService> fileManager,
                          std::shared_ptr<RealtimePublisher> realtime);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FileManagerController::listEntries,
                  "/api/file-manager/contexts/{1}/{2}/entries",
                  drogon::Get);
    ADD_METHOD_TO(FileManagerController::uploadFile,
                  "/api/file-manager/contexts/{1}/{2}/files",
                  drogon::Post);
    ADD_METHOD_TO(FileManagerController::createFolder,
                  "/api/file-manager/contexts/{1}/{2}/folders",
                  drogon::Post);
    ADD_METHOD_TO(FileManagerController::renameEntry,
                  "/api/file-manager/contexts/{1}/{2}/entries/{3}",
                  drogon::Patch);
    ADD_METHOD_TO(FileManagerController::moveEntryToTrash,
                  "/api/file-manager/contexts/{1}/{2}/entries/{3}",
                  drogon::Delete);
    ADD_METHOD_TO(FileManagerController::downloadEntry,
                  "/api/file-manager/contexts/{1}/{2}/entries/{3}/download",
                  drogon::Get);
    ADD_METHOD_TO(FileManagerController::listTrash,
                  "/api/file-manager/contexts/{1}/{2}/trash",
                  drogon::Get);
    ADD_METHOD_TO(FileManagerController::restoreTrashEntry,
                  "/api/file-manager/contexts/{1}/{2}/trash/{3}/restore",
                  drogon::Post);
    ADD_METHOD_TO(FileManagerController::permanentlyDeleteTrashEntry,
                  "/api/file-manager/contexts/{1}/{2}/trash/{3}",
                  drogon::Delete);
    METHOD_LIST_END

    void listEntries(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     std::string contextType,
                     std::string contextId);
    void uploadFile(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string contextType,
                    std::string contextId);
    void createFolder(const drogon::HttpRequestPtr& request,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      std::string contextType,
                      std::string contextId);
    void renameEntry(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     std::string contextType,
                     std::string contextId,
                     std::string entryId);
    void moveEntryToTrash(const drogon::HttpRequestPtr& request,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          std::string contextType,
                          std::string contextId,
                          std::string entryId);
    void downloadEntry(const drogon::HttpRequestPtr& request,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       std::string contextType,
                       std::string contextId,
                       std::string entryId);
    void listTrash(const drogon::HttpRequestPtr& request,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   std::string contextType,
                   std::string contextId);
    void restoreTrashEntry(const drogon::HttpRequestPtr& request,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           std::string contextType,
                           std::string contextId,
                           std::string trashEntryId);
    void permanentlyDeleteTrashEntry(const drogon::HttpRequestPtr& request,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                     std::string contextType,
                                     std::string contextId,
                                     std::string trashEntryId);

  private:
    std::shared_ptr<infra::FileManagerService> file_manager_;
    std::shared_ptr<RealtimePublisher> realtime_;
};

}  // namespace schoolmanager::adapters

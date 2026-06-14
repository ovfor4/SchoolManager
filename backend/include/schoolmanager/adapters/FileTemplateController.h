#pragma once

#include "schoolmanager/application/FileTemplateUseCases.h"

#include <drogon/HttpController.h>

#include <functional>
#include <memory>

namespace schoolmanager::adapters {

class FileTemplateController : public drogon::HttpController<FileTemplateController, false> {
  public:
    explicit FileTemplateController(
        std::shared_ptr<application::FileTemplateUseCases> fileTemplates);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FileTemplateController::generate,
                  "/api/file-templates/generate",
                  drogon::Post);
    METHOD_LIST_END

    void generate(const drogon::HttpRequestPtr& request,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

  private:
    std::shared_ptr<application::FileTemplateUseCases> file_templates_;
};

}  // namespace schoolmanager::adapters

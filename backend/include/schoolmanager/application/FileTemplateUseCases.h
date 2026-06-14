#pragma once

#include "schoolmanager/application/DocumentFormats.h"
#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/FileManagerService.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StudentDataRepository.h"
#include "schoolmanager/infra/ZipArchiveWriter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace schoolmanager::application {

struct TemplateSource {
    std::string file_name;
    std::string mime_type;
    std::string content;
};

struct FileTemplateGenerateRequest {
    std::string template_entry_id;
    std::vector<std::string> student_ids;
    std::optional<std::string> source_format;
    std::optional<std::string> export_format;
};

struct GeneratedFileDownload {
    std::string file_name;
    std::string mime_type;
    std::string content;
};

class FileTemplateUseCases {
  public:
    FileTemplateUseCases(std::shared_ptr<infra::FileManagerService> fileManager,
                         std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                         std::shared_ptr<infra::StudentDataRepository> studentData,
                         std::shared_ptr<DocumentFormatRegistry> documents,
                         infra::ZipArchiveWriter zipWriter);

    DocumentFormatCapabilities capabilities() const;
    GeneratedFileDownload generate(const FileTemplateGenerateRequest& request);

  private:
    std::shared_ptr<infra::FileManagerService> file_manager_;
    std::shared_ptr<infra::SchoolIndexRepository> school_index_;
    std::shared_ptr<infra::StudentDataRepository> student_data_;
    std::shared_ptr<DocumentFormatRegistry> documents_;
    infra::ZipArchiveWriter zip_writer_;

    TemplateSource loadTemplate(std::string_view templateEntryId);
    DocumentRenderVariables variablesForStudent(
        const domain::StudentSummary& student,
        const std::vector<domain::StudentInfoDefinition>& definitions);
};

}  // namespace schoolmanager::application

#pragma once

#include "schoolmanager/application/TemplateProcessor.h"
#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/FileManagerService.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StudentDataRepository.h"
#include "schoolmanager/infra/ZipArchiveWriter.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace schoolmanager::application {

struct FileTemplateGenerateRequest {
    std::string template_entry_id;
    std::vector<std::string> student_ids;
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
                         std::shared_ptr<TemplateProcessorRegistry> processors,
                         infra::ZipArchiveWriter zipWriter);

    GeneratedFileDownload generate(const FileTemplateGenerateRequest& request);

  private:
    std::shared_ptr<infra::FileManagerService> file_manager_;
    std::shared_ptr<infra::SchoolIndexRepository> school_index_;
    std::shared_ptr<infra::StudentDataRepository> student_data_;
    std::shared_ptr<TemplateProcessorRegistry> processors_;
    infra::ZipArchiveWriter zip_writer_;

    TemplateSource loadTemplate(std::string_view templateEntryId);
    TemplateRenderVariables variablesForStudent(
        const domain::StudentSummary& student,
        const std::vector<domain::StudentInfoDefinition>& definitions);
};

std::shared_ptr<TemplateProcessorRegistry> createDefaultTemplateProcessorRegistry();

}  // namespace schoolmanager::application

#include "schoolmanager/application/FileTemplateUseCases.h"

#include "schoolmanager/application/ApplicationError.h"
#include "schoolmanager/application/StudentInfoFields.h"
#include "schoolmanager/config/Constants.h"
#include "schoolmanager/domain/Models.h"
#include "schoolmanager/infra/StoragePaths.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace schoolmanager::application {

namespace {

struct RenderedStudentFile {
    std::string file_name;
    std::string mime_type;
    std::string content;
};

domain::FileContext globalTemplatesContext()
{
    return domain::FileContext{
        .type = std::string(config::globalTemplatesContextType),
        .id = std::string(config::defaultGlobalTemplatesContextId),
    };
}

std::string readFileContent(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read template file");
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string nameWithCopySuffix(const std::string& name, int copyIndex)
{
    const auto path = std::filesystem::path(name);
    const auto extension = path.extension().string();
    const auto stem = path.stem().string();
    if (extension.empty() || stem.empty()) {
        return name + " (" + std::to_string(copyIndex) + ")";
    }
    return stem + " (" + std::to_string(copyIndex) + ")" + extension;
}

std::string templateStem(std::string_view templateName)
{
    const auto path = std::filesystem::path(std::string(templateName));
    auto stem = path.stem().string();
    if (stem.empty()) {
        stem = path.filename().string();
    }
    if (stem.empty()) {
        stem = "template";
    }
    return stem;
}

std::string generatedFileName(const domain::StudentSummary& student,
                              std::string_view templateName,
                              std::string_view extension)
{
    return infra::sanitizeFileName(student.display_name + "-" + templateStem(templateName) +
                                   std::string(extension));
}

std::string uniqueFileName(std::string name, std::unordered_set<std::string>& usedNames)
{
    if (usedNames.insert(name).second) {
        return name;
    }

    for (int copyIndex = 2; copyIndex < 10000; ++copyIndex) {
        auto candidate = nameWithCopySuffix(name, copyIndex);
        if (usedNames.insert(candidate).second) {
            return candidate;
        }
    }
    throw std::runtime_error("failed to find available generated file name");
}

ApplicationError fileManagerLoadError(const infra::FileManagerError& error)
{
    if (error.code() == infra::FileManagerErrorCode::NotFound) {
        return ApplicationError(ApplicationErrorCode::NotFound, "template file not found");
    }
    return ApplicationError(ApplicationErrorCode::BadRequest, error.what());
}

}  // namespace

FileTemplateUseCases::FileTemplateUseCases(
    std::shared_ptr<infra::FileManagerService> fileManager,
    std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
    std::shared_ptr<infra::StudentDataRepository> studentData,
    std::shared_ptr<DocumentFormatRegistry> documents,
    infra::ZipArchiveWriter zipWriter)
    : file_manager_(std::move(fileManager)),
      school_index_(std::move(schoolIndex)),
      student_data_(std::move(studentData)),
      documents_(std::move(documents)),
      zip_writer_(std::move(zipWriter))
{
}

DocumentFormatCapabilities FileTemplateUseCases::capabilities() const
{
    if (!documents_) {
        throw std::runtime_error("document format registry is required");
    }
    return documents_->capabilities();
}

GeneratedFileDownload FileTemplateUseCases::generate(const FileTemplateGenerateRequest& request)
{
    if (!domain::isSafeId(request.template_entry_id)) {
        throw ApplicationError(ApplicationErrorCode::BadRequest, "template_entry_id is invalid");
    }
    if (!documents_) {
        throw std::runtime_error("document format registry is required");
    }

    const auto source = loadTemplate(request.template_entry_id);
    const auto& sourceFormat = documents_->resolveSourceFormat(request.source_format,
                                                               source.file_name,
                                                               source.mime_type);
    const auto& exportFormat = documents_->resolveExportFormat(sourceFormat, request.export_format);

    if (!sourceFormat.supports_student_variables) {
        return GeneratedFileDownload{
            .file_name = source.file_name,
            .mime_type = source.mime_type.empty() ? std::string(config::octetStreamMimeType)
                                                  : source.mime_type,
            .content = source.content,
        };
    }

    if (request.student_ids.empty()) {
        throw ApplicationError(ApplicationErrorCode::BadRequest, "student_ids is required");
    }

    const auto& renderer = documents_->variableRendererFor(sourceFormat);
    const auto definitions = school_index_->listStudentInfoDefinitions();

    std::vector<RenderedStudentFile> files;
    files.reserve(request.student_ids.size());
    std::unordered_set<std::string> usedFileNames;

    for (const auto& studentId : request.student_ids) {
        if (!domain::isSafeId(studentId)) {
            throw ApplicationError(ApplicationErrorCode::BadRequest, "student id is invalid");
        }

        const auto student = school_index_->getStudent(studentId);
        if (!student) {
            throw ApplicationError(ApplicationErrorCode::NotFound, "student not found");
        }

        const auto rendered = renderer.render(source.content, variablesForStudent(*student, definitions));
        const auto processed = documents_->convert(sourceFormat, exportFormat, rendered);
        files.push_back(RenderedStudentFile{
            .file_name = uniqueFileName(
                generatedFileName(*student, source.file_name, processed.extension),
                usedFileNames),
            .mime_type = processed.mime_type,
            .content = processed.content,
        });
    }

    if (files.size() == 1) {
        auto& file = files.front();
        return GeneratedFileDownload{
            .file_name = std::move(file.file_name),
            .mime_type = std::move(file.mime_type),
            .content = std::move(file.content),
        };
    }

    std::vector<infra::ZipArchiveEntry> zipEntries;
    zipEntries.reserve(files.size());
    for (auto& file : files) {
        zipEntries.push_back(infra::ZipArchiveEntry{
            .name = std::move(file.file_name),
            .content = std::move(file.content),
        });
    }

    return GeneratedFileDownload{
        .file_name = std::string(config::generatedTemplatesZipFileName),
        .mime_type = std::string(config::zipMimeType),
        .content = zip_writer_.write(zipEntries),
    };
}

TemplateSource FileTemplateUseCases::loadTemplate(std::string_view templateEntryId)
{
    try {
        const auto context = globalTemplatesContext();
        const auto entry = file_manager_->getEntry(context, templateEntryId);
        if (!entry || entry->status != config::fileEntryStatusActive) {
            throw ApplicationError(ApplicationErrorCode::NotFound, "template file not found");
        }
        if (entry->kind != config::fileEntryKindFile) {
            throw ApplicationError(ApplicationErrorCode::BadRequest, "template entry must be a file");
        }

        const auto download = file_manager_->downloadFile(context, templateEntryId);
        return TemplateSource{
            .file_name = download.entry.name,
            .mime_type = download.entry.mime_type,
            .content = readFileContent(download.path),
        };
    } catch (const infra::FileManagerError& error) {
        throw fileManagerLoadError(error);
    }
}

DocumentRenderVariables FileTemplateUseCases::variablesForStudent(
    const domain::StudentSummary& student,
    const std::vector<domain::StudentInfoDefinition>& definitions)
{
    DocumentRenderVariables variables;
    variables.values_by_name.emplace(std::string(config::templateReservedStudentNameVariable),
                                     student.display_name);
    for (const auto& field : mergeStudentInfoFields(definitions,
                                                    student_data_->listStudentInfoValues(student.id))) {
        variables.values_by_name.emplace(field.name, field.value);
    }
    return variables;
}

}  // namespace schoolmanager::application

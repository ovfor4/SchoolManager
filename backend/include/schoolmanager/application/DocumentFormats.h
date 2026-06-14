#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace schoolmanager::application {

struct DocumentExportFormatInfo {
    std::string id;
    std::string label;
    std::string extension;
    std::string mime_type;
    std::string implementation_format_id;
};

struct DocumentSourceFormatInfo {
    std::string id;
    std::string label;
    std::vector<std::string> extensions;
    std::vector<std::string> mime_types;
    std::string default_export_format;
    std::vector<std::string> export_format_ids;
    bool supports_student_variables = false;
    std::string implementation_format_id;
};

struct DocumentFormatCapabilities {
    std::string default_source_format;
    std::vector<DocumentSourceFormatInfo> source_formats;
    std::vector<DocumentExportFormatInfo> export_formats;
};

struct RenderedDocument {
    std::string content;
    std::string format_id;
    std::string mime_type;
    std::string extension;
};

struct DocumentRenderVariables {
    std::unordered_map<std::string, std::string> values_by_name;
};

class DocumentVariableRenderer {
  public:
    virtual ~DocumentVariableRenderer() = default;

    [[nodiscard]] virtual std::string_view sourceFormatId() const = 0;
    [[nodiscard]] virtual std::string render(std::string_view sourceContent,
                                             const DocumentRenderVariables& variables) const = 0;
};

class DocumentConverter {
  public:
    virtual ~DocumentConverter() = default;

    [[nodiscard]] virtual std::string_view sourceFormatId() const = 0;
    [[nodiscard]] virtual std::string_view targetFormatId() const = 0;
    [[nodiscard]] virtual std::string convert(std::string_view content) const = 0;
};

class DocumentFormatRegistry {
  public:
    void setDefaultSourceFormat(std::string formatId);
    void registerSourceFormat(DocumentSourceFormatInfo format);
    void registerExportFormat(DocumentExportFormatInfo format);
    void registerVariableRenderer(std::unique_ptr<DocumentVariableRenderer> renderer);
    void registerConverter(std::unique_ptr<DocumentConverter> converter);

    [[nodiscard]] DocumentFormatCapabilities capabilities() const;
    [[nodiscard]] const DocumentSourceFormatInfo& resolveSourceFormat(
        const std::optional<std::string>& requestedFormat,
        std::string_view fileName,
        std::string_view mimeType) const;
    [[nodiscard]] const DocumentExportFormatInfo& resolveExportFormat(
        const DocumentSourceFormatInfo& sourceFormat,
        const std::optional<std::string>& requestedFormat) const;
    [[nodiscard]] const DocumentVariableRenderer& variableRendererFor(
        const DocumentSourceFormatInfo& sourceFormat) const;
    [[nodiscard]] RenderedDocument convert(const DocumentSourceFormatInfo& sourceFormat,
                                           const DocumentExportFormatInfo& targetFormat,
                                           std::string_view content) const;

  private:
    std::string default_source_format_id_;
    std::vector<DocumentSourceFormatInfo> source_formats_;
    std::vector<DocumentExportFormatInfo> export_formats_;
    std::vector<std::unique_ptr<DocumentVariableRenderer>> renderers_;
    std::vector<std::unique_ptr<DocumentConverter>> converters_;

    [[nodiscard]] const DocumentSourceFormatInfo& sourceFormatById(
        std::string_view formatId) const;
    [[nodiscard]] const DocumentExportFormatInfo& exportFormatById(
        std::string_view formatId) const;
    [[nodiscard]] const DocumentSourceFormatInfo& detectSourceFormat(
        std::string_view fileName,
        std::string_view mimeType) const;
};

std::shared_ptr<DocumentFormatRegistry> createDefaultDocumentFormatRegistry();

}  // namespace schoolmanager::application

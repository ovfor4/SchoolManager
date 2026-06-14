#include "schoolmanager/application/TemplateProcessor.h"

#include "schoolmanager/application/ApplicationError.h"
#include "schoolmanager/application/TemplateTextRenderer.h"
#include "schoolmanager/config/Constants.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace schoolmanager::application {

namespace {

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string mimeTypeWithoutParameters(std::string_view value)
{
    auto text = std::string(value);
    const auto semicolon = text.find(';');
    if (semicolon != std::string::npos) {
        text.resize(semicolon);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.pop_back();
    }
    return lowerAscii(std::move(text));
}

std::string lowerExtension(std::string_view fileName)
{
    return lowerAscii(std::filesystem::path(std::string(fileName)).extension().string());
}

class PlainTextTemplateProcessor final : public TemplateProcessor {
  public:
    bool supports(std::string_view fileName, std::string_view mimeType) const override
    {
        const auto mime = mimeTypeWithoutParameters(mimeType);
        if (mime == config::plainTextMimeType) {
            return true;
        }
        return lowerExtension(fileName) == ".txt";
    }

    ProcessedTemplate render(const TemplateSource& source,
                             const TemplateRenderVariables& variables) const override
    {
        return ProcessedTemplate{
            .content = renderTextTemplate(source.content, variables.values_by_name),
            .mime_type = std::string(config::plainTextMimeType),
        };
    }
};

}  // namespace

void TemplateProcessorRegistry::registerProcessor(std::unique_ptr<TemplateProcessor> processor)
{
    if (!processor) {
        throw std::runtime_error("template processor is required");
    }
    processors_.push_back(std::move(processor));
}

const TemplateProcessor& TemplateProcessorRegistry::processorFor(std::string_view fileName,
                                                                 std::string_view mimeType) const
{
    for (const auto& processor : processors_) {
        if (processor->supports(fileName, mimeType)) {
            return *processor;
        }
    }
    throw ApplicationError(ApplicationErrorCode::BadRequest, "unsupported template type");
}

std::unique_ptr<TemplateProcessor> createPlainTextTemplateProcessor()
{
    return std::make_unique<PlainTextTemplateProcessor>();
}

}  // namespace schoolmanager::application

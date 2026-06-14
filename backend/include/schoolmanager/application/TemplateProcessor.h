#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace schoolmanager::application {

struct TemplateSource {
    std::string file_name;
    std::string mime_type;
    std::string content;
};

struct TemplateRenderVariables {
    std::unordered_map<std::string, std::string> values_by_name;
};

struct ProcessedTemplate {
    std::string content;
    std::string mime_type;
};

class TemplateProcessor {
  public:
    virtual ~TemplateProcessor() = default;

    virtual bool supports(std::string_view fileName, std::string_view mimeType) const = 0;
    virtual ProcessedTemplate render(const TemplateSource& source,
                                     const TemplateRenderVariables& variables) const = 0;
};

class TemplateProcessorRegistry {
  public:
    void registerProcessor(std::unique_ptr<TemplateProcessor> processor);
    const TemplateProcessor& processorFor(std::string_view fileName,
                                          std::string_view mimeType) const;

  private:
    std::vector<std::unique_ptr<TemplateProcessor>> processors_;
};

std::unique_ptr<TemplateProcessor> createPlainTextTemplateProcessor();

}  // namespace schoolmanager::application

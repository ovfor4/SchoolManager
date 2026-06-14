#include "schoolmanager/application/TemplateTextRenderer.h"

#include "schoolmanager/domain/Models.h"

namespace schoolmanager::application {

namespace {

bool isEscapedBrace(std::string_view text, std::size_t index)
{
    return text[index] == '\\' && index + 1 < text.size() &&
           (text[index + 1] == '{' || text[index + 1] == '}');
}

}  // namespace

std::string renderTextTemplate(
    std::string_view templateText,
    const std::unordered_map<std::string, std::string>& variables)
{
    std::string rendered;
    rendered.reserve(templateText.size());

    for (std::size_t index = 0; index < templateText.size();) {
        if (isEscapedBrace(templateText, index)) {
            rendered.push_back(templateText[index + 1]);
            index += 2;
            continue;
        }

        if (templateText[index] != '{') {
            rendered.push_back(templateText[index]);
            ++index;
            continue;
        }

        std::size_t close = index + 1;
        bool closed = false;
        while (close < templateText.size()) {
            if (isEscapedBrace(templateText, close)) {
                close += 2;
                continue;
            }
            if (templateText[close] == '}') {
                closed = true;
                break;
            }
            ++close;
        }

        if (!closed) {
            rendered.append(templateText.substr(index));
            break;
        }

        const auto variableName = templateText.substr(index + 1, close - index - 1);
        const auto original = templateText.substr(index, close - index + 1);
        if (variableName.empty() || !domain::isSafeId(variableName)) {
            rendered.append(original);
            index = close + 1;
            continue;
        }

        const auto value = variables.find(std::string(variableName));
        if (value != variables.end() && !value->second.empty()) {
            rendered.append(value->second);
        } else {
            rendered.append(original);
        }
        index = close + 1;
    }

    return rendered;
}

}  // namespace schoolmanager::application

#include "schoolmanager/application/StudentInfoFields.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace schoolmanager::application {

std::vector<domain::StudentInfoField> mergeStudentInfoFields(
    const std::vector<domain::StudentInfoDefinition>& definitions,
    const std::vector<domain::StudentInfoValue>& values)
{
    std::unordered_map<std::string, domain::StudentInfoValue> valuesByFieldId;
    for (const auto& value : values) {
        valuesByFieldId.emplace(value.field_id, value);
    }

    std::vector<domain::StudentInfoField> fields;
    fields.reserve(definitions.size());
    for (const auto& definition : definitions) {
        domain::StudentInfoField field{
            .id = definition.id,
            .name = definition.name,
            .display_name = definition.display_name,
            .value_type = definition.value_type,
            .value = "",
            .updated_at = definition.updated_at,
        };

        const auto value = valuesByFieldId.find(definition.id);
        if (value != valuesByFieldId.end()) {
            if (domain::isValidStudentInfoValue(definition.value_type, value->second.value)) {
                field.value = value->second.value;
            }
            field.updated_at = std::max(definition.updated_at, value->second.updated_at);
        }

        fields.push_back(std::move(field));
    }
    return fields;
}

domain::StudentInfoField mergeStudentInfoField(
    const domain::StudentInfoDefinition& definition,
    const domain::StudentInfoValue& value)
{
    auto fields = mergeStudentInfoFields({definition}, {value});
    return fields.front();
}

}  // namespace schoolmanager::application

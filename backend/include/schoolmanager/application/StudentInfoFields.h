#pragma once

#include "schoolmanager/domain/Models.h"

#include <vector>

namespace schoolmanager::application {

std::vector<domain::StudentInfoField> mergeStudentInfoFields(
    const std::vector<domain::StudentInfoDefinition>& definitions,
    const std::vector<domain::StudentInfoValue>& values);

domain::StudentInfoField mergeStudentInfoField(
    const domain::StudentInfoDefinition& definition,
    const domain::StudentInfoValue& value);

}  // namespace schoolmanager::application

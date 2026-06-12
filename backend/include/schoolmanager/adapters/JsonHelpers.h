#pragma once

#include "schoolmanager/domain/Models.h"

#include <json/json.h>

#include <string>
#include <vector>

namespace schoolmanager::adapters {

Json::Value toJson(const domain::StudentSummary& student);
Json::Value toJson(const domain::Grade& grade);
Json::Value toJson(const domain::StoredFile& file);

Json::Value toJsonArray(const std::vector<domain::StudentSummary>& students);
Json::Value toJsonArray(const std::vector<domain::Grade>& grades);
Json::Value toJsonArray(const std::vector<domain::StoredFile>& files);

std::string writeJsonCompact(const Json::Value& value);
std::string scalarToString(const Json::Value& value);

}  // namespace schoolmanager::adapters

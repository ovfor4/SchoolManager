#pragma once

#include "schoolmanager/adapters/BroadcastHub.h"
#include "schoolmanager/domain/Models.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace schoolmanager::adapters {

class RealtimePublisher {
  public:
    explicit RealtimePublisher(std::shared_ptr<BroadcastHub> hub);

    void studentCreated(const domain::StudentSummary& student);
    void studentUpdated(const domain::StudentSummary& student,
                        const std::map<std::string, std::string>& changes);
    void studentDeleted(std::string_view studentId);

    void studentInfoDefinitionCreated(const domain::StudentInfoDefinition& field);
    void studentInfoDefinitionUpdated(const domain::StudentInfoDefinition& field,
                                      const std::map<std::string, std::string>& changes);
    void studentInfoDefinitionDeleted(std::string_view fieldId);

    void studentInfoUpdated(std::string_view studentId,
                            const domain::StudentInfoField& field,
                            const std::map<std::string, std::string>& changes);

    void gradeCreated(std::string_view studentId, const domain::Grade& grade);
    void gradeUpdated(std::string_view studentId,
                      const domain::Grade& grade,
                      const std::map<std::string, std::string>& changes);
    void gradeDeleted(std::string_view studentId, std::string_view gradeId);

    void fileManagerChanged(const domain::FileContext& context, std::string_view action);

  private:
    std::shared_ptr<BroadcastHub> hub_;
};

}  // namespace schoolmanager::adapters

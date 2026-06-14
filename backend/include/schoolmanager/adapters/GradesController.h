#pragma once

#include "schoolmanager/adapters/RealtimePublisher.h"
#include "schoolmanager/application/GradeUseCases.h"

#include <drogon/HttpController.h>

#include <functional>
#include <memory>
#include <string>

namespace schoolmanager::adapters {

class GradesController : public drogon::HttpController<GradesController, false> {
  public:
    GradesController(std::shared_ptr<application::GradeUseCases> grades,
                     std::shared_ptr<RealtimePublisher> realtime);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(GradesController::listGrades, "/api/students/{1}/grades", drogon::Get);
    ADD_METHOD_TO(GradesController::createGrade, "/api/students/{1}/grades", drogon::Post);
    ADD_METHOD_TO(GradesController::patchGrade,
                  "/api/students/{1}/grades/{2}",
                  drogon::Patch);
    ADD_METHOD_TO(GradesController::deleteGrade,
                  "/api/students/{1}/grades/{2}",
                  drogon::Delete);
    METHOD_LIST_END

    void listGrades(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string studentId);
    void createGrade(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     std::string studentId);
    void patchGrade(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string studentId,
                    std::string gradeId);
    void deleteGrade(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     std::string studentId,
                     std::string gradeId);

  private:
    std::shared_ptr<application::GradeUseCases> grades_;
    std::shared_ptr<RealtimePublisher> realtime_;
};

}  // namespace schoolmanager::adapters

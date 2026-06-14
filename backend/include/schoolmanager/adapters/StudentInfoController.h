#pragma once

#include "schoolmanager/adapters/RealtimePublisher.h"
#include "schoolmanager/application/StudentInfoUseCases.h"

#include <drogon/HttpController.h>

#include <functional>
#include <memory>
#include <string>

namespace schoolmanager::adapters {

class StudentInfoController : public drogon::HttpController<StudentInfoController, false> {
  public:
    StudentInfoController(std::shared_ptr<application::StudentInfoUseCases> studentInfo,
                          std::shared_ptr<RealtimePublisher> realtime);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StudentInfoController::listDefinitions,
                  "/api/student-info-fields",
                  drogon::Get);
    ADD_METHOD_TO(StudentInfoController::createDefinition,
                  "/api/student-info-fields",
                  drogon::Post);
    ADD_METHOD_TO(StudentInfoController::patchDefinition,
                  "/api/student-info-fields/{1}",
                  drogon::Patch);
    ADD_METHOD_TO(StudentInfoController::deleteDefinition,
                  "/api/student-info-fields/{1}",
                  drogon::Delete);
    ADD_METHOD_TO(StudentInfoController::listStudentFields,
                  "/api/students/{1}/info-fields",
                  drogon::Get);
    ADD_METHOD_TO(StudentInfoController::patchStudentValue,
                  "/api/students/{1}/info-fields/{2}",
                  drogon::Patch);
    METHOD_LIST_END

    void listDefinitions(const drogon::HttpRequestPtr& request,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void createDefinition(const drogon::HttpRequestPtr& request,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patchDefinition(const drogon::HttpRequestPtr& request,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         std::string fieldId);
    void deleteDefinition(const drogon::HttpRequestPtr& request,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          std::string fieldId);
    void listStudentFields(const drogon::HttpRequestPtr& request,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           std::string studentId);
    void patchStudentValue(const drogon::HttpRequestPtr& request,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           std::string studentId,
                           std::string fieldId);

  private:
    std::shared_ptr<application::StudentInfoUseCases> student_info_;
    std::shared_ptr<RealtimePublisher> realtime_;
};

}  // namespace schoolmanager::adapters

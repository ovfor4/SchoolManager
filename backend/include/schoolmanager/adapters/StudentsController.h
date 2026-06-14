#pragma once

#include "schoolmanager/adapters/RealtimePublisher.h"
#include "schoolmanager/application/StudentUseCases.h"

#include <drogon/HttpController.h>

#include <functional>
#include <memory>
#include <string>

namespace schoolmanager::adapters {

class StudentsController : public drogon::HttpController<StudentsController, false> {
  public:
    StudentsController(std::shared_ptr<application::StudentUseCases> students,
                       std::shared_ptr<RealtimePublisher> realtime);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StudentsController::listStudents, "/api/students", drogon::Get);
    ADD_METHOD_TO(StudentsController::createStudent, "/api/students", drogon::Post);
    ADD_METHOD_TO(StudentsController::getStudent, "/api/students/{1}", drogon::Get);
    ADD_METHOD_TO(StudentsController::patchStudent, "/api/students/{1}", drogon::Patch);
    ADD_METHOD_TO(StudentsController::deleteStudent, "/api/students/{1}", drogon::Delete);
    METHOD_LIST_END

    void listStudents(const drogon::HttpRequestPtr& request,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void createStudent(const drogon::HttpRequestPtr& request,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getStudent(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string studentId);
    void patchStudent(const drogon::HttpRequestPtr& request,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      std::string studentId);
    void deleteStudent(const drogon::HttpRequestPtr& request,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       std::string studentId);

  private:
    std::shared_ptr<application::StudentUseCases> students_;
    std::shared_ptr<RealtimePublisher> realtime_;
};

}  // namespace schoolmanager::adapters

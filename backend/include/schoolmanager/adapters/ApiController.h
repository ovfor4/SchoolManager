#pragma once

#include "schoolmanager/adapters/BroadcastHub.h"
#include "schoolmanager/infra/SchoolIndexRepository.h"
#include "schoolmanager/infra/StudentDataRepository.h"

#include <drogon/HttpController.h>

#include <filesystem>
#include <memory>

namespace schoolmanager::adapters {

class ApiController : public drogon::HttpController<ApiController, false> {
  public:
    ApiController(std::shared_ptr<infra::SchoolIndexRepository> schoolIndex,
                  std::shared_ptr<infra::StudentDataRepository> studentData,
                  std::shared_ptr<BroadcastHub> broadcastHub,
                  std::filesystem::path openApiPath);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ApiController::health, "/api/health", drogon::Get);
    ADD_METHOD_TO(ApiController::openApi, "/api/openapi.json", drogon::Get);
    ADD_METHOD_TO(ApiController::listStudents, "/api/students", drogon::Get);
    ADD_METHOD_TO(ApiController::createStudent, "/api/students", drogon::Post);
    ADD_METHOD_TO(ApiController::listStudentInfoDefinitions,
                  "/api/student-info-fields",
                  drogon::Get);
    ADD_METHOD_TO(ApiController::createStudentInfoDefinition,
                  "/api/student-info-fields",
                  drogon::Post);
    ADD_METHOD_TO(ApiController::patchStudentInfoDefinition,
                  "/api/student-info-fields/{1}",
                  drogon::Patch);
    ADD_METHOD_TO(ApiController::deleteStudentInfoDefinition,
                  "/api/student-info-fields/{1}",
                  drogon::Delete);
    ADD_METHOD_TO(ApiController::getStudent, "/api/students/{1}", drogon::Get);
    ADD_METHOD_TO(ApiController::patchStudent, "/api/students/{1}", drogon::Patch);
    ADD_METHOD_TO(ApiController::deleteStudent, "/api/students/{1}", drogon::Delete);
    ADD_METHOD_TO(ApiController::listStudentInfoFields,
                  "/api/students/{1}/info-fields",
                  drogon::Get);
    ADD_METHOD_TO(ApiController::patchStudentInfoValue,
                  "/api/students/{1}/info-fields/{2}",
                  drogon::Patch);
    ADD_METHOD_TO(ApiController::listGrades, "/api/students/{1}/grades", drogon::Get);
    ADD_METHOD_TO(ApiController::createGrade, "/api/students/{1}/grades", drogon::Post);
    ADD_METHOD_TO(ApiController::patchGrade, "/api/students/{1}/grades/{2}", drogon::Patch);
    ADD_METHOD_TO(ApiController::deleteGrade, "/api/students/{1}/grades/{2}", drogon::Delete);
    ADD_METHOD_TO(ApiController::listFiles, "/api/students/{1}/files", drogon::Get);
    ADD_METHOD_TO(ApiController::uploadFile, "/api/students/{1}/files", drogon::Post);
    ADD_METHOD_TO(ApiController::downloadFile,
                  "/api/students/{1}/files/{2}/download",
                  drogon::Get);
    METHOD_LIST_END

    void health(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void openApi(const drogon::HttpRequestPtr& request,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void listStudents(const drogon::HttpRequestPtr& request,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void createStudent(const drogon::HttpRequestPtr& request,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void listStudentInfoDefinitions(const drogon::HttpRequestPtr& request,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void createStudentInfoDefinition(const drogon::HttpRequestPtr& request,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void patchStudentInfoDefinition(const drogon::HttpRequestPtr& request,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                    std::string fieldId);
    void deleteStudentInfoDefinition(const drogon::HttpRequestPtr& request,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                     std::string fieldId);
    void getStudent(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string studentId);
    void patchStudent(const drogon::HttpRequestPtr& request,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      std::string studentId);
    void deleteStudent(const drogon::HttpRequestPtr& request,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       std::string studentId);
    void listStudentInfoFields(const drogon::HttpRequestPtr& request,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                               std::string studentId);
    void patchStudentInfoValue(const drogon::HttpRequestPtr& request,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                               std::string studentId,
                               std::string fieldId);
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
    void listFiles(const drogon::HttpRequestPtr& request,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   std::string studentId);
    void uploadFile(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string studentId);
    void downloadFile(const drogon::HttpRequestPtr& request,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      std::string studentId,
                      std::string fileId);

  private:
    std::shared_ptr<infra::SchoolIndexRepository> school_index_;
    std::shared_ptr<infra::StudentDataRepository> student_data_;
    std::shared_ptr<BroadcastHub> broadcast_hub_;
    std::filesystem::path open_api_path_;

    bool studentExists(std::string_view studentId);
};

}  // namespace schoolmanager::adapters

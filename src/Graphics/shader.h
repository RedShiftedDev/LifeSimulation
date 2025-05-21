#pragma once
#include <glad/glad.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
  Shader(std::filesystem::path vertexPath, std::filesystem::path fragmentPath);
  ~Shader();

  void use() const;
  void reload();

  void setBool(std::string_view name, bool v) const;
  void setInt(std::string_view name, int v) const;
  void setFloat(std::string_view name, float v) const;
  void setVec3(std::string_view name, glm::vec3 const &v) const;
  void setMat4(std::string_view name, glm::mat4 const &m) const;
  GLuint getProgramID() const { return programID; }

private:
  GLuint programID;
  std::filesystem::path vertexPath, fragmentPath;
  mutable std::unordered_map<std::string, GLint> uniformLocationCache;
  GLint getUniformLocation(std::string_view name) const;
  static GLuint buildProgramFromFiles(std::filesystem::path const &vsPath,
                                      std::filesystem::path const &fsPath);
  static void checkCompileErrors(GLuint id, std::string_view type);
};

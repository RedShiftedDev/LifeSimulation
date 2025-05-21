#include "shader.h"
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

Shader::Shader(std::filesystem::path vertexPath, std::filesystem::path fragmentPath)
    : vertexPath(std::move(vertexPath)), fragmentPath(std::move(fragmentPath)) {
  programID = buildProgramFromFiles(this->vertexPath, this->fragmentPath);
}

Shader::~Shader() { glDeleteProgram(programID); }

void Shader::use() const { glUseProgram(programID); }

void Shader::reload() {
  glDeleteProgram(programID);
  uniformLocationCache.clear();
  programID = buildProgramFromFiles(vertexPath, fragmentPath);
}

void Shader::setBool(std::string_view name, bool value) const {
  glUniform1i(getUniformLocation(name), (int)value);
}

void Shader::setInt(std::string_view name, int value) const {
  glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(std::string_view name, float value) const {
  glUniform1f(getUniformLocation(name), value);
}

void Shader::setVec3(std::string_view name, const glm::vec3 &value) const {
  glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setMat4(std::string_view name, const glm::mat4 &mat) const {
  glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

GLint Shader::getUniformLocation(std::string_view name) const {
  auto it = uniformLocationCache.find(std::string{name});
  if (it != uniformLocationCache.end()) {
    return it->second;
  }
  GLint location = glGetUniformLocation(programID, name.data());
  uniformLocationCache.emplace(std::string{name}, location);
  return location;
}

GLuint Shader::buildProgramFromFiles(const std::filesystem::path &vsPath,
                                     const std::filesystem::path &fsPath) {
  // Load vertex shader code
  std::ifstream vertexFile(vsPath);
  if (!vertexFile.is_open()) {
    throw std::runtime_error("Failed to open vertex shader file: " + vsPath.string());
  }
  std::stringstream vertexStream;
  vertexStream << vertexFile.rdbuf();
  std::string vertexCode = vertexStream.str();

  // Load fragment shader code
  std::ifstream fragmentFile(fsPath);
  if (!fragmentFile.is_open()) {
    throw std::runtime_error("Failed to open fragment shader file: " + fsPath.string());
  }
  std::stringstream fragmentStream;
  fragmentStream << fragmentFile.rdbuf();
  std::string fragmentCode = fragmentStream.str();

  const char *vShaderCode = vertexCode.c_str();
  const char *fShaderCode = fragmentCode.c_str();

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vShaderCode, nullptr);
  glCompileShader(vertexShader);
  checkCompileErrors(vertexShader, "VERTEX");

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fShaderCode, nullptr);
  glCompileShader(fragmentShader);
  checkCompileErrors(fragmentShader, "FRAGMENT");

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  checkCompileErrors(program, "PROGRAM");

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

void Shader::checkCompileErrors(GLuint id, std::string_view type) {
  GLint success;
  char infoLog[1024];
  if (type != "PROGRAM") {
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (success == 0) {
      glGetShaderInfoLog(id, 1024, nullptr, infoLog);
      throw std::runtime_error(std::string("ERROR::SHADER_COMPILATION_ERROR of type: ") +
                               std::string(type) + "\n" + infoLog);
    }
  } else {
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (success == 0) {
      glGetProgramInfoLog(id, 1024, nullptr, infoLog);
      throw std::runtime_error(std::string("ERROR::PROGRAM_LINKING_ERROR of type: ") +
                               std::string(type) + "\n" + infoLog);
    }
  }
}

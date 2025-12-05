#pragma once

#include <string>
#include <GL/glew.h>

class ShaderLoader {
public:

    // Loads a shader program from two file paths
    static GLuint createShaderProgram(const char *vertexPath,
                                      const char *fragmentPath);

    // Internal helpers
    static GLuint createShader(GLenum shaderType, const char *filePath);
    static std::string loadFileAsString(const char *filePath);
};

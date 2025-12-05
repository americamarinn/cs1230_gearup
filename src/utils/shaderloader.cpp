#include "shaderloader.h"

#include <iostream>
#include <fstream>
#include <sstream>

// Load entire file as string
std::string ShaderLoader::loadFileAsString(const char *filePath) {
    std::ifstream file(filePath);
    if (!file.good()) {
        std::cerr << "❌ ShaderLoader ERROR: Could not open file: "
                  << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Compile a vertex or fragment shader
GLuint ShaderLoader::createShader(GLenum shaderType, const char *filePath) {
    std::string srcString = loadFileAsString(filePath);
    if (srcString.empty()) {
        std::cerr << "❌ ShaderLoader: Shader source is EMPTY for: "
                  << filePath << std::endl;
        return 0;
    }

    const char *src = srcString.c_str();

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    // Check compile errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        GLint logSize = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);

        std::string log(logSize, ' ');
        glGetShaderInfoLog(shader, logSize, nullptr, log.data());

        std::cerr << "❌ ShaderLoader: Error compiling shader " << filePath << "\n"
                  << log << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    std::cout << "✔ Shader compiled: " << filePath << std::endl;
    return shader;
}

// Link vertex + fragment into program
GLuint ShaderLoader::createShaderProgram(const char *vertexPath,
                                         const char *fragmentPath) {
    GLuint vert = createShader(GL_VERTEX_SHADER, vertexPath);
    GLuint frag = createShader(GL_FRAGMENT_SHADER, fragmentPath);

    if (vert == 0 || frag == 0) {
        std::cerr << "❌ ShaderLoader: Could NOT create shader program!\n";
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    // Check link errors
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        GLint logSize = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);

        std::string log(logSize, ' ');
        glGetProgramInfoLog(program, logSize, nullptr, log.data());

        std::cerr << "❌ ShaderLoader: Program link error:\n"
                  << log << std::endl;

        glDeleteProgram(program);
        return 0;
    }

    // Safe to delete shaders now
    glDeleteShader(vert);
    glDeleteShader(frag);

    std::cout << "✔ Linked program: " << vertexPath << " + " << fragmentPath << std::endl;

    return program;
}

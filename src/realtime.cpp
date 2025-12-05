#include "realtime.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "settings.h"
#include "utils/cube.h"
#include "utils/cone.h"
#include "utils/sphere.h"
#include "utils/cylinder.h"
#include "shaderloader.h"
#include "scenedata.h"
#include "utils/debug.h"

Realtime::Realtime(QWidget *parent)
    : QOpenGLWidget(parent),
    m_mouseDown(false),
    m_camPos(0.f, 2.f, 5.f),
    m_camLook(0.f, 0.f, -1.f),
    m_camUp(0.f, 1.f, 0.f)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_keyMap[Qt::Key_W] = false;
    m_keyMap[Qt::Key_A] = false;
    m_keyMap[Qt::Key_S] = false;
    m_keyMap[Qt::Key_D] = false;
    m_keyMap[Qt::Key_Q] = false;
    m_keyMap[Qt::Key_E] = false;
    m_keyMap[Qt::Key_Space] = false;
    m_keyMap[Qt::Key_Control] = false;
}

void Realtime::finish() {
    makeCurrent();

    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteProgram(m_gbufferShader);
    glDeleteProgram(m_deferredShader);

    for (auto& kv : m_shapeVAOs) {
        glDeleteVertexArrays(1, &kv.second);
    }

    doneCurrent();
}

void Realtime::sceneChanged() {
    bool success = SceneParser::parse(settings.sceneFilePath, m_renderData);
    if (!success) {
        std::cerr << "Error parsing scene: " << settings.sceneFilePath << std::endl;
        return;
    }

    // 🔧 FIX: Update camera to match the new scene
    SceneCameraData& camData = m_renderData.cameraData;
    m_camera.setViewMatrix(camData.pos, camData.look, camData.up);

    // Update projection with new height angle/planes
    float aspectRatio = (float)width() / (float)height();
    m_camera.setProjectionMatrix(aspectRatio, settings.nearPlane, settings.farPlane, camData.heightAngle);

    update();
}

void Realtime::settingsChanged() {
    // If settings affect projection (near/far), update it here
    float aspectRatio = (float)width() / (float)height();
    m_camera.setProjectionMatrix(aspectRatio, settings.nearPlane, settings.farPlane, m_renderData.cameraData.heightAngle);
    update();
}

void Realtime::initializeGL() {
    glewInit();

    m_defaultFBO = defaultFramebufferObject();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // 1. Create Shape VAOs
    std::vector<PrimitiveType> types = {
        PrimitiveType::PRIMITIVE_CUBE,
        PrimitiveType::PRIMITIVE_SPHERE,
        PrimitiveType::PRIMITIVE_CYLINDER,
        PrimitiveType::PRIMITIVE_CONE
    };

    for (PrimitiveType t : types) {
        std::vector<float> data;

        // Initialize the shape with parameters from settings before generating
        if (t == PrimitiveType::PRIMITIVE_CUBE) {
            Cube c;
            c.updateParams(settings.shapeParameter1, settings.shapeParameter2);
            data = c.generateShape();
        }
        else if (t == PrimitiveType::PRIMITIVE_SPHERE) {
            Sphere s;
            s.updateParams(settings.shapeParameter1, settings.shapeParameter2);
            data = s.generateShape();
        }
        else if (t == PrimitiveType::PRIMITIVE_CYLINDER) {
            Cylinder c;
            c.updateParams(settings.shapeParameter1, settings.shapeParameter2);
            data = c.generateShape();
        }
        else if (t == PrimitiveType::PRIMITIVE_CONE) {
            Cone c;
            c.updateParams(settings.shapeParameter1, settings.shapeParameter2);
            data = c.generateShape();
        }

        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

        // Position (Layout 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

        // Normal (Layout 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_shapeVAOs[t] = vao;
        m_shapeVertexCounts[t] = data.size() / 6;
    }

    // 2. Initialize Shaders
    m_gbufferShader = ShaderLoader::createShaderProgram(
        "resources/shaders/gbuffer.vert",
        "resources/shaders/gbuffer.frag");

    m_deferredShader = ShaderLoader::createShaderProgram(
        "resources/shaders/fullscreen_quad.vert",
        "resources/shaders/deferredLighting.frag");

    // Set samplers for deferred shader once
    glUseProgram(m_deferredShader);
    glUniform1i(glGetUniformLocation(m_deferredShader, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(m_deferredShader, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(m_deferredShader, "gAlbedo"), 2);
    glUniform1i(glGetUniformLocation(m_deferredShader, "gEmissive"), 3);
    glUseProgram(0);

    // 3. Initialize Fullscreen Quad
    float quadVerts[] = {
        // pos        // uv
        -1.f, -1.f,   0.f, 0.f,
        1.f, -1.f,   1.f, 0.f,
        -1.f,  1.f,   0.f, 1.f,
        1.f,  1.f,   1.f, 1.f,
        -1.f,  1.f,   0.f, 1.f,
        1.f, -1.f,   1.f, 0.f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // Pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 4. Init GBuffer
    m_gbuffer.init(width() * devicePixelRatio(), height() * devicePixelRatio());

    m_elapsedTimer.start();
    m_timer = startTimer(16);
}

// void Realtime::initializeGL() {
//     glewInit();

//     m_defaultFBO = defaultFramebufferObject();

//     glEnable(GL_DEPTH_TEST);
//     glEnable(GL_CULL_FACE);

//     //start of create shape
//     // --------------------
//     std::vector<PrimitiveType> types = {
//         PrimitiveType::PRIMITIVE_CUBE,
//         PrimitiveType::PRIMITIVE_SPHERE,
//         PrimitiveType::PRIMITIVE_CYLINDER,
//         PrimitiveType::PRIMITIVE_CONE
//     };

//     for (PrimitiveType t : types) {
//         std::vector<float> data;

//         // 🔧 FIX: Initialize the shape with parameters before generating!
//         if (t == PrimitiveType::PRIMITIVE_CUBE) {
//             Cube c;
//             c.updateParams(settings.shapeParameter1, settings.shapeParameter2);
//             data = c.generateShape();
//         }
//         else if (t == PrimitiveType::PRIMITIVE_SPHERE) {
//             Sphere s;
//             s.updateParams(settings.shapeParameter1, settings.shapeParameter2);
//             data = s.generateShape();
//         }
//         else if (t == PrimitiveType::PRIMITIVE_CYLINDER) {
//             Cylinder c;
//             c.updateParams(settings.shapeParameter1, settings.shapeParameter2);
//             data = c.generateShape();
//         }
//         else if (t == PrimitiveType::PRIMITIVE_CONE) {
//             Cone c;
//             c.updateParams(settings.shapeParameter1, settings.shapeParameter2);
//             data = c.generateShape();
//         }

//         // Debug check to make sure we actually have data now
//         if (data.empty()) {
//             std::cerr << "⚠️ WARNING: Shape data is empty for primitive type " << (int)t << std::endl;
//         }

//         GLuint vao, vbo;
//         glGenVertexArrays(1, &vao);
//         glBindVertexArray(vao);

//         glGenBuffers(1, &vbo);
//         glBindBuffer(GL_ARRAY_BUFFER, vbo);
//         glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

//         // Pos
//         glEnableVertexAttribArray(0);
//         glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
//         // Norm
//         glEnableVertexAttribArray(1);
//         glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));

//         glBindVertexArray(0);
//         glBindBuffer(GL_ARRAY_BUFFER, 0);

//         m_shapeVAOs[t] = vao;
//         m_shapeVertexCounts[t] = data.size() / 6;
//     }

//      // --------------------
//     //end of create shape

//     // 2. Initialize Shaders
//     m_gbufferShader = ShaderLoader::createShaderProgram(
//         "resources/shaders/gbuffer.vert",
//         "resources/shaders/gbuffer.frag");

//     m_deferredShader = ShaderLoader::createShaderProgram(
//         "resources/shaders/fullscreen_quad.vert",
//         "resources/shaders/deferredLighting.frag");

//     // Set samplers for deferred shader once
//     glUseProgram(m_deferredShader);
//     glUniform1i(glGetUniformLocation(m_deferredShader, "gPosition"), 0);
//     glUniform1i(glGetUniformLocation(m_deferredShader, "gNormal"), 1);
//     glUniform1i(glGetUniformLocation(m_deferredShader, "gAlbedo"), 2);
//     glUniform1i(glGetUniformLocation(m_deferredShader, "gEmissive"), 3);
//     glUseProgram(0);

//     // 3. Initialize Quad
//     float quadVerts[] = {
//         // pos        // uv
//         -1.f, -1.f,   0.f, 0.f,
//         1.f, -1.f,   1.f, 0.f,
//         -1.f,  1.f,   0.f, 1.f,
//         1.f,  1.f,   1.f, 1.f,
//         -1.f, 1.f,   0.f, 1.f,
//         1.f, -1.f,   1.f, 0.f
//     };

//     glGenVertexArrays(1, &m_quadVAO);
//     glGenBuffers(1, &m_quadVBO);
//     glBindVertexArray(m_quadVAO);
//     glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

//     glEnableVertexAttribArray(0); // Pos
//     glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
//     glEnableVertexAttribArray(1); // UV
//     glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

//     glBindVertexArray(0);
//     glBindBuffer(GL_ARRAY_BUFFER, 0);

//     // 4. Init GBuffer
//     m_gbuffer.init(width() * devicePixelRatio(), height() * devicePixelRatio());

//     m_elapsedTimer.start();
//     m_timer = startTimer(16);
// }

void Realtime::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    m_gbuffer.resize(w * devicePixelRatio(), h * devicePixelRatio());

    float aspectRatio = (float)w / (float)h;
    m_camera.setProjectionMatrix(
        aspectRatio,
        settings.nearPlane,
        settings.farPlane,
        m_renderData.cameraData.heightAngle
        );
}


void Realtime::paintGL() {
    // Delta time
    float dt = m_elapsedTimer.restart() * 0.001f;
    updateCamera(dt);

    // ==========================================
    // PHASE 1: GEOMETRY PASS
    // ==========================================
    m_gbuffer.bindForWriting();

    glViewport(0, 0, m_gbuffer.getWidth(), m_gbuffer.getHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(m_gbufferShader);

    // Camera Matrices
    glUniformMatrix4fv(glGetUniformLocation(m_gbufferShader, "view"), 1, GL_FALSE, &m_camera.getViewMatrix()[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(m_gbufferShader, "proj"), 1, GL_FALSE, &m_camera.getProjMatrix()[0][0]);

    // Draw Shapes
    for (const auto& shape : m_renderData.shapes) {
        glUniformMatrix4fv(glGetUniformLocation(m_gbufferShader, "model"), 1, GL_FALSE, &shape.ctm[0][0]);
        glUniform3fv(glGetUniformLocation(m_gbufferShader, "albedo"), 1, &shape.primitive.material.cDiffuse[0]);
        glUniform3fv(glGetUniformLocation(m_gbufferShader, "emissive"), 1, &shape.primitive.material.cEmissive[0]);

        glBindVertexArray(m_shapeVAOs[shape.primitive.type]);
        glDrawArrays(GL_TRIANGLES, 0, m_shapeVertexCounts[shape.primitive.type]);
        glBindVertexArray(0);
    }

    glDisable(GL_DEPTH_TEST);

    // 🔧 CRITICAL FIX: Get the valid default FBO ID from Qt
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

    // ==========================================
    // PHASE 2: LIGHTING PASS
    // ==========================================
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT); // No depth clear needed for full screen quad

    glUseProgram(m_deferredShader);

    // Bind Textures
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getPositionTex());
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getNormalTex());
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getAlbedoTex());
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getEmissiveTex());

    // Uniforms
    glm::vec3 camPos = m_camera.getPosition();
    glUniform3fv(glGetUniformLocation(m_deferredShader, "camPos"), 1, &camPos[0]);

    // Lights
    int numLights = std::min((int)m_renderData.lights.size(), 8);
    glUniform1i(glGetUniformLocation(m_deferredShader, "numLights"), numLights);
    glUniform1f(glGetUniformLocation(m_deferredShader, "k_a"), m_renderData.globalData.ka);
    glUniform1f(glGetUniformLocation(m_deferredShader, "k_d"), m_renderData.globalData.kd);
    glUniform1f(glGetUniformLocation(m_deferredShader, "k_s"), m_renderData.globalData.ks);

    for (int i = 0; i < numLights; i++) {
        std::string base = "lights[" + std::to_string(i) + "]";
        const auto& light = m_renderData.lights[i];

        glUniform1i(glGetUniformLocation(m_deferredShader, (base + ".type").c_str()), static_cast<int>(light.type));
        glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".pos").c_str()), 1, &light.pos[0]);
        glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".dir").c_str()), 1, &light.dir[0]);
        glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".color").c_str()), 1, &light.color[0]);
        glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".atten").c_str()), 1, &light.function[0]);
        glUniform1f(glGetUniformLocation(m_deferredShader, (base + ".angle").c_str()), light.angle);
        glUniform1f(glGetUniformLocation(m_deferredShader, (base + ".penumbra").c_str()), light.penumbra);
    }

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}

// void Realtime::paintGL() {
//     // Delta time
//     float dt = m_elapsedTimer.restart() * 0.001f;
//     updateCamera(dt);

//     // ==========================================
//     // PHASE 1: GEOMETRY PASS
//     // ==========================================
//     m_gbuffer.bindForWriting();
//     GL_CHECK(); // Check if FBO binding worked

//     glViewport(0, 0, m_gbuffer.getWidth(), m_gbuffer.getHeight());
//     glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//     glEnable(GL_DEPTH_TEST);
//     GL_CHECK();

//     glUseProgram(m_gbufferShader);

//     // Camera Matrices
//     glUniformMatrix4fv(glGetUniformLocation(m_gbufferShader, "view"), 1, GL_FALSE, &m_camera.getViewMatrix()[0][0]);
//     glUniformMatrix4fv(glGetUniformLocation(m_gbufferShader, "proj"), 1, GL_FALSE, &m_camera.getProjMatrix()[0][0]);
//     GL_CHECK();

//     // Draw Shapes
//     for (const auto& shape : m_renderData.shapes) {
//         glUniformMatrix4fv(glGetUniformLocation(m_gbufferShader, "model"), 1, GL_FALSE, &shape.ctm[0][0]);
//         glUniform3fv(glGetUniformLocation(m_gbufferShader, "albedo"), 1, &shape.primitive.material.cDiffuse[0]);
//         glUniform3fv(glGetUniformLocation(m_gbufferShader, "emissive"), 1, &shape.primitive.material.cEmissive[0]);

//         //this is a test for emissive buffer test
//         // glm::vec3 fakeGlow(0.0f, 1.0f, 0.0f);
//         // glUniform3fv(glGetUniformLocation(m_gbufferShader, "emissive"), 1, &fakeGlow[0]);

//         glBindVertexArray(m_shapeVAOs[shape.primitive.type]);
//         glDrawArrays(GL_TRIANGLES, 0, m_shapeVertexCounts[shape.primitive.type]);
//         glBindVertexArray(0);
//         GL_CHECK(); // Check if draw call failed
//     }

//     glDisable(GL_DEPTH_TEST);
//     //added a fix here
//     glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
//     GL_CHECK();

//     // ==========================================
//     // PHASE 2: LIGHTING PASS
//     // ==========================================
//     glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
//     glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//     glUseProgram(m_deferredShader);

//     // Bind Textures
//     glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getPositionTex());
//     glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getNormalTex());
//     glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getAlbedoTex());
//     glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_gbuffer.getEmissiveTex());

//     //GL ERROR 0x506 from line below
//     GL_CHECK(); // Check if textures are valid

//     // Uniforms
//     glm::vec3 camPos = m_camera.getPosition();
//     glUniform3fv(glGetUniformLocation(m_deferredShader, "camPos"), 1, &camPos[0]);

//     // Debug Light Count
//     int numLights = std::min((int)m_renderData.lights.size(), 8);
//     // std::cout << "Rendering with " << numLights << " lights" << std::endl; // Uncomment to verify lights exist

//     glUniform1i(glGetUniformLocation(m_deferredShader, "numLights"), numLights);
//     glUniform1f(glGetUniformLocation(m_deferredShader, "k_a"), m_renderData.globalData.ka);
//     glUniform1f(glGetUniformLocation(m_deferredShader, "k_d"), m_renderData.globalData.kd);
//     glUniform1f(glGetUniformLocation(m_deferredShader, "k_s"), m_renderData.globalData.ks);

//     for (int i = 0; i < numLights; i++) {
//         std::string base = "lights[" + std::to_string(i) + "]";
//         const auto& light = m_renderData.lights[i];

//         // Quick check to ensure locations are valid (returns -1 if invalid, which isn't an error, but good to know)
//         // GLint loc = glGetUniformLocation(m_deferredShader, (base + ".type").c_str());

//         glUniform1i(glGetUniformLocation(m_deferredShader, (base + ".type").c_str()), static_cast<int>(light.type));
//         glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".pos").c_str()), 1, &light.pos[0]);
//         glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".dir").c_str()), 1, &light.dir[0]);
//         glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".color").c_str()), 1, &light.color[0]);
//         glUniform3fv(glGetUniformLocation(m_deferredShader, (base + ".atten").c_str()), 1, &light.function[0]);
//         glUniform1f(glGetUniformLocation(m_deferredShader, (base + ".angle").c_str()), light.angle);
//         glUniform1f(glGetUniformLocation(m_deferredShader, (base + ".penumbra").c_str()), light.penumbra);
//     }
//     GL_CHECK(); // Check if uniforms crashed it

//     glBindVertexArray(m_quadVAO);
//     glDrawArrays(GL_TRIANGLES, 0, 6);
//     glBindVertexArray(0);

//     //GL ERROR 0x506 from line below
//     GL_CHECK(); // Check if full screen quad failed

//     glUseProgram(0);
// }

void Realtime::timerEvent(QTimerEvent*) {
    update();
}

void Realtime::keyPressEvent(QKeyEvent *event) {
    m_keyMap[Qt::Key(event->key())] = true;
}

void Realtime::keyReleaseEvent(QKeyEvent *event) {
    m_keyMap[Qt::Key(event->key())] = false;
}

void Realtime::mousePressEvent(QMouseEvent *event) {
    if (event->buttons().testFlag(Qt::LeftButton)) {
        m_mouseDown = true;
        m_prevMousePos = glm::vec2(event->position().x(), event->position().y());
    }
}

void Realtime::mouseReleaseEvent(QMouseEvent *event) {
    if (!event->buttons().testFlag(Qt::LeftButton))
        m_mouseDown = false;
}

void Realtime::mouseMoveEvent(QMouseEvent *event) {
    if (!m_mouseDown) return;

    glm::vec2 cur(event->position().x(), event->position().y());
    glm::vec2 delta = cur - m_prevMousePos;
    m_prevMousePos = cur;

    const float sensitivity = 0.005f;
    float yaw   = -delta.x * sensitivity;
    float pitch = -delta.y * sensitivity;

    if (yaw != 0.f) m_camera.rotateAroundUp(yaw);
    if (pitch != 0.f) m_camera.rotateAroundRight(pitch);
}

void Realtime::updateCamera(float dt) {
    const float speed = 5.f;
    glm::vec3 moveDir(0.f);

    glm::vec3 look = m_camera.getLook();
    glm::vec3 up = glm::vec3(0,1,0);
    glm::vec3 right = glm::normalize(glm::cross(look, up));
    // Flatten look for movement to prevent flying up/down
    glm::vec3 flatLook = glm::normalize(glm::vec3(look.x, 0, look.z));

    if (m_keyMap[Qt::Key_W]) moveDir += flatLook;
    if (m_keyMap[Qt::Key_S]) moveDir -= flatLook;
    if (m_keyMap[Qt::Key_D]) moveDir += right;
    if (m_keyMap[Qt::Key_A]) moveDir -= right;
    if (m_keyMap[Qt::Key_Space]) moveDir += up;
    if (m_keyMap[Qt::Key_Control]) moveDir -= up;

    if (glm::length(moveDir) > 0.f) {
        moveDir = glm::normalize(moveDir);
        m_camera.translate(moveDir * speed * dt);
    }
}

void Realtime::saveViewportImage(const std::string &filePath) {
    makeCurrent();

    int fixedWidth = 1024;
    int fixedHeight = 768;

    GLuint fbo, texture, rbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fixedWidth, fixedHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fixedWidth, fixedHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // Save previous state
    int oldW = width() * devicePixelRatio();
    int oldH = height() * devicePixelRatio();

    // Temporarily resize GBuffer and Camera for snapshot
    m_gbuffer.resize(fixedWidth, fixedHeight);
    float aspectRatio = (float)fixedWidth / (float)fixedHeight;
    m_camera.setProjectionMatrix(aspectRatio, settings.nearPlane, settings.farPlane, m_renderData.cameraData.heightAngle);

    // Render to FBO
    glViewport(0, 0, fixedWidth, fixedHeight);
    // Bind default FBO momentarily to fool paintGL into rendering to our bound FBO (since paintGL binds m_defaultFBO)
    GLint oldDefault = m_defaultFBO;
    m_defaultFBO = fbo;

    paintGL();

    // Read pixels
    std::vector<unsigned char> pixels(fixedWidth * fixedHeight * 3);
    glReadPixels(0, 0, fixedWidth, fixedHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Restore state
    m_defaultFBO = oldDefault;
    glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFBO);
    glDeleteTextures(1, &texture);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);

    // Revert resize
    resizeGL(width(), height());

    QImage img(pixels.data(), fixedWidth, fixedHeight, QImage::Format_RGB888);
    img.mirrored().save(QString::fromStdString(filePath));
}



// #include "realtime.h"

// #include <QCoreApplication>
// #include <QMouseEvent>
// #include <QKeyEvent>
// #include <iostream>
// #include "settings.h"
// #include <algorithm>
// #include <glm/gtc/matrix_transform.hpp>

// // ================== Rendering the Scene!

// Realtime::Realtime(QWidget *parent)
//     : QOpenGLWidget(parent)
// {
//     m_prev_mouse_pos = glm::vec2(size().width()/2, size().height()/2);
//     setMouseTracking(true);
//     setFocusPolicy(Qt::StrongFocus);

//     m_keyMap[Qt::Key_W]       = false;
//     m_keyMap[Qt::Key_A]       = false;
//     m_keyMap[Qt::Key_S]       = false;
//     m_keyMap[Qt::Key_D]       = false;
//     m_keyMap[Qt::Key_Control] = false;
//     m_keyMap[Qt::Key_Space]   = false;

//     // If you must use this function, do not edit anything above this
// }

// void Realtime::finish() {
//     killTimer(m_timer);
//     this->makeCurrent();

//     // Students: anything requiring OpenGL calls when the program exits should be done here
    
//     //
//     cleanupVAOs();
//     if (m_shader) glDeleteProgram(m_shader);

//     this->doneCurrent();
// }


// //Loads scene + sets up camera based on m_renderdata and also generates VAOs for each shape (setup method)
// void Realtime::loadScene() {
//     //Start from scratch in terms of VBOs and VAOs
//     cleanupVAOs();
    
//     //Using my lab 4 scene parser to produce a renderdata with shapes, lights, camera, and globals
//     std::string scenePath = settings.sceneFilePath;
//     if (scenePath.empty()) {
//         std::cout << "[Realtime] No scene file selected" << std::endl;
//         return;
//     }
    
//     //parsing the scene
//     bool success = SceneParser::parse(scenePath, m_renderData);
//     if (!success) {
//         std::cerr << "[Realtime] Failed to parse scene" << std::endl;
//         return;
//     }

//     // Set up camera from scene data
//     m_camera.setViewMatrix(
//         glm::vec3(m_renderData.cameraData.pos),
//         glm::vec3(m_renderData.cameraData.look),
//         glm::vec3(m_renderData.cameraData.up)
//         );

//     float aspect = (size().height() > 0) ? float(size().width()) / float(size().height()) : 1.f;
//     m_camera.setProjectionMatrix(aspect, settings.nearPlane, settings.farPlane,
//                                  m_renderData.cameraData.heightAngle);
    
//     //cache
//     glm::vec3 pos  = glm::vec3(m_renderData.cameraData.pos);
//     glm::vec3 look = glm::vec3(m_renderData.cameraData.look);
//     glm::vec3 up   = glm::vec3(m_renderData.cameraData.up);

//     // Save camera state (like a cache)
//     m_camPos  = pos;
//     m_camLook = look;
//     m_camUp   = up;

//     m_camera.setViewMatrix(m_camPos, m_camLook, m_camUp);

//     //Generating VAOS
//     generateShapeVAOs();

//     std::cout << "[Realtime] Scene loaded: " << m_renderData.shapes.size()
//               << " shapes, " << m_renderData.lights.size() << " lights" << std::endl;
// }

// //Takes care of setting up the VAO and VBO stuff per shape! (Copied straight from lecture 1 VAO VBO)
// void Realtime::generateShapeVAOs() {
//     m_shapeVAOs.clear();
    
//     //generate shape data gives us the array with all the shape's information
//     for (const RenderShapeData& shape : m_renderData.shapes) {
//         std::vector<float> vertexData = generateShapeData(
//             shape.primitive.type,
//             settings.shapeParameter1,
//             settings.shapeParameter2
//             );
        
//         //the VAO struct
//         //      GLuint vao;
//         //      GLuint vbo;
//         //      int vertexCount;
//         ShapeVAO shapeVAO;
        
        
//         // Allocate and assign a Vertex Array Object to our handle
//         glGenVertexArrays(1, &shapeVAO.vao);
        
//         // Allocate and assign a VBO
//         glGenBuffers(1, &shapeVAO.vbo);

//         // Bind VAO
//         glBindVertexArray(shapeVAO.vao);
//         // Bind VBO
//         glBindBuffer(GL_ARRAY_BUFFER, shapeVAO.vbo);
        
//         // Copy the vertex data from the 'triangle' array to our buffer
//         glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float),
//                      vertexData.data(), GL_STATIC_DRAW);
        
//         //LAYOUT of VAO
//         // Position attribute
//         glEnableVertexAttribArray(0);
//         glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
//         // Normal attribute
//         glEnableVertexAttribArray(1);
//         glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
//                               (void*)(3 * sizeof(float)));
        
        
//         glBindBuffer(GL_ARRAY_BUFFER, 0);
//         glBindVertexArray(0);

//         shapeVAO.vertexCount = vertexData.size() / 6;
//         m_shapeVAOs.push_back(shapeVAO);
//     }
// }

// //Generate shape data (positions and normals only) based on the two parameters, not much here! generateShape ->
// //THIS IS THE ARRAY STUFF FROM LECTURE 3 POSITIONS -> 3 NROMALS
// //USED IN VAO VBO
// std::vector<float> Realtime::generateShapeData(PrimitiveType type, int param1, int param2) {
//     int p1 = std::max(1, param1);
//     int p2 = std::max(3, param2);

//     switch (type) {
//     case PrimitiveType::PRIMITIVE_CUBE: {
//         Cube cube;
//         cube.updateParams(p1, p2);
//         return cube.generateShape();
//     }
//     case PrimitiveType::PRIMITIVE_CONE: {
//         Cone cone;
//         cone.updateParams(p1, p2);
//         return cone.generateShape();
//     }
//     case PrimitiveType::PRIMITIVE_CYLINDER: {
//         Cylinder cylinder;
//         cylinder.updateParams(p1, p2);
//         return cylinder.generateShape();
//     }
//     case PrimitiveType::PRIMITIVE_SPHERE: {
//         Sphere sphere;
//         sphere.updateParams(p1, p2);
//         return sphere.generateShape();
//     }
//     default:
//         return std::vector<float>();
//     }
// }

// //Just deletes all VBOs and VAOs
// void Realtime::cleanupVAOs() {
//     for (ShapeVAO& shapeVAO : m_shapeVAOs) {
//         glDeleteBuffers(1, &shapeVAO.vbo);
//         glDeleteVertexArrays(1, &shapeVAO.vao);
//     }
//     m_shapeVAOs.clear();
// }



// void Realtime::initializeGL() {
//     m_devicePixelRatio = this->devicePixelRatio();

//     m_timer = startTimer(1000/60);
//     m_elapsedTimer.start();

//     glewExperimental = GL_TRUE;
//     GLenum err = glewInit();
//     if (err != GLEW_OK) {
//         std::cerr << "Error while initializing GL: " << glewGetErrorString(err) << std::endl;
//     }
//     std::cout << "Initialized GL: Version " << glewGetString(GLEW_VERSION) << std::endl;

//     glEnable(GL_DEPTH_TEST);
//     glEnable(GL_CULL_FACE);
//     glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);
    
//     //My stuff:
//     // Load shader
//     m_shader = ShaderLoader::createShaderProgram(
//         ":/resources/shaders/default.vert",
//         ":/resources/shaders/default.frag"
//         );

//     glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
// }


// void Realtime::paintGL() {
//     //Clearing the screen
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//     if (!m_shader) return;
    
//     //activating the shader program
//     glUseProgram(m_shader);

    
//     glm::mat4 view = m_camera.getViewMatrix();
//     glm::mat4 proj = m_camera.getProjMatrix();
    
//     //Passing matrices to the vertex shader uniforms (VIEW and PROJ)
//     GLint viewLoc = glGetUniformLocation(m_shader, "view");
//     GLint projLoc = glGetUniformLocation(m_shader, "proj");
//     glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
//     glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);
    
    
//     // --- camera position --- to frag shader for reflection/view vectors for specular
//     GLint camPosLoc = glGetUniformLocation(m_shader, "camPos");
//     glUniform3fv(camPosLoc, 1, &m_camPos[0]);

//     // --- global coefficients --- to frag shader
//     const SceneGlobalData &G = m_renderData.globalData;
//     glUniform1f(glGetUniformLocation(m_shader, "k_a"), G.ka);
//     glUniform1f(glGetUniformLocation(m_shader, "k_d"), G.kd);
//     glUniform1f(glGetUniformLocation(m_shader, "k_s"), G.ks);


//     // --- lights --- to frag shader
//     int numLights = std::min<int>(m_renderData.lights.size(), 8);
//     glUniform1i(glGetUniformLocation(m_shader, "numLights"), numLights);
    
//     //Frag shader will iterate over numLights (capped at 8)
    
//     //Lights stuff - ask chat how to make sense of this!
//     for (int i = 0; i < numLights; ++i) {
//         //getting info from scenelightdata
//         const SceneLightData &L = m_renderData.lights[i];

//         // 0 point, 1 directional, 2 spot
//         //getting the type
//         int typeInt = 0;
//         if (L.type == LightType::LIGHT_DIRECTIONAL) typeInt = 1;
//         else if (L.type == LightType::LIGHT_SPOT)  typeInt = 2;
        
        
//         //packs each light into the uniform array lights[i]
//         //lambda metohd to create the unfirmos and send them
//         //this gets the id, helper to simplify code
//         auto loc = [&](const std::string &field) {
//             std::string name = "lights[" + std::to_string(i) + "]." + field;
//             return glGetUniformLocation(m_shader, name.c_str());
//         };
//         //loc ("thing") -> id
//         glUniform1i(loc("type"), typeInt);
//         glUniform3fv(loc("color"), 1, &glm::vec3(L.color)[0]);
//         glUniform3fv(loc("pos"),   1, &glm::vec3(L.pos)[0]);
//         glUniform3fv(loc("dir"),   1, &glm::vec3(L.dir)[0]);
//         glUniform3fv(loc("atten"), 1, &glm::vec3(L.function)[0]);
//         glUniform1f(loc("angle"),      L.angle);
//         glUniform1f(loc("penumbra"),   L.penumbra);
//     }
    
//     //IF ASKED ABOUT THIS YOU CAN DISREGARD. THIS IS NOT USED ANYMORE, IT WAS USED FOR TESTING AND FORGOT TO DELTE
//     // single point light: use first light in the scene
//     GLint lightPosLoc   = glGetUniformLocation(m_shader, "lightPos");
//     GLint lightColorLoc = glGetUniformLocation(m_shader, "lightColor");

//     glm::vec3 lightPos(5.f, 5.f, 5.f);
//     glm::vec3 lightCol(1.f, 1.f, 1.f);

//     if (!m_renderData.lights.empty()) {
//         const SceneLightData &L0 = m_renderData.lights[0];
//         lightPos = glm::vec3(L0.pos);
//         lightCol = glm::vec3(L0.color);
//     }

//     glUniform3fv(lightPosLoc,   1, &lightPos[0]);
//     glUniform3fv(lightColorLoc, 1, &lightCol[0]);
//     //END OF FORGOT TO DELETE AREA
    
    
//     // "For every object we want to render" or for i in the shapes array
//     for (size_t i = 0; i < m_renderData.shapes.size(); i++) {
//         //getting shape data and VAO
//         const RenderShapeData &shape = m_renderData.shapes[i];
//         const ShapeVAO &vao = m_shapeVAOs[i];

//         // sending model matrix to VERTEX shader to transform object-space vertices into world space.
//         GLint modelLoc = glGetUniformLocation(m_shader, "model");
//         glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &shape.ctm[0][0]);
        
//         //sending lighitng information from phong to frag shader
//         // material
//         const SceneMaterial &mat = shape.primitive.material;

//         glm::vec3 cA = glm::vec3(mat.cAmbient);
//         glm::vec3 cD = glm::vec3(mat.cDiffuse);
//         glm::vec3 cS = glm::vec3(mat.cSpecular);

//         glUniform3fv(glGetUniformLocation(m_shader, "cAmbient"),  1, &cA[0]);
//         glUniform3fv(glGetUniformLocation(m_shader, "cDiffuse"),  1, &cD[0]);
//         glUniform3fv(glGetUniformLocation(m_shader, "cSpecular"), 1, &cS[0]);
//         glUniform1f(glGetUniformLocation(m_shader, "shininess"),  mat.shininess);
        
//         //AT THIS POINT we are done with sending stuff to the shaders.
//         //Below is a continuation of the standard draw program seen in lecture.
        
//         //FROM LECTURE

//         // Bind VAO for object
//         glBindVertexArray(vao.vao);
        
//         //Draw the currently-bound VAO with the currently-bound shader
//         glDrawArrays(GL_TRIANGLES, 0, vao.vertexCount);
        
//         //It’s good practice to unbind when done rendering
//         glBindVertexArray(0);
//     }

//     glUseProgram(0);
// }

// void Realtime::resizeGL(int w, int h) {
//     // Tells OpenGL how big the screen is
//     glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

//     // Students: anything requiring OpenGL calls when the program starts should be done here

//     // Keep camera projection in sync with window size
//     float aspect = (h > 0) ? float(w) / float(h) : 1.f;
//     float fovY = !m_renderData.shapes.empty()
//                      ? m_renderData.cameraData.heightAngle //If scene loaded we use this angle
//                      : 45.f * 3.1415926535f / 180.f;

//     m_camera.setProjectionMatrix(
//         aspect,
//         settings.nearPlane,
//         settings.farPlane,
//         fovY
//         );
// }

// void Realtime::sceneChanged() {
//     makeCurrent();
//     loadScene();
//     doneCurrent();
//     update();


// }


// // NEAR PLANE + FAR PLANE
// // NEAR PLANE should chop off front
// void Realtime::settingsChanged() {

//     makeCurrent();

//     if (!m_renderData.shapes.empty()) {
//         generateShapeVAOs();
//     }

//     float aspect = float(size().width()) / float(size().height());
//     float fovY = !m_renderData.shapes.empty()
//                      ? m_renderData.cameraData.heightAngle
//                      : 45.f * 3.1415926535f / 180.f;

//     m_camera.setProjectionMatrix(aspect, settings.nearPlane, settings.farPlane, fovY);

//     doneCurrent();
//     update();

// }

// // ================== Camera Movement!

// // Helper to rotate vector v around (normalized) axis by angle (radians)
// //based on rodrigues
// static glm::vec3 rotateAroundAxis(const glm::vec3 &v,
//                                   const glm::vec3 &axis,
//                                   float angle)
// {
//     glm::vec3 a = glm::normalize(axis);
//     float c = std::cos(angle);
//     float s = std::sin(angle);

//     // Rodrigues' rotation formula
//     return v * c
//            + glm::cross(a, v) * s
//            + a * glm::dot(a, v) * (1.f - c);
// }


// void Realtime::keyPressEvent(QKeyEvent *event) {
//     m_keyMap[Qt::Key(event->key())] = true;
// }

// void Realtime::keyReleaseEvent(QKeyEvent *event) {
//     m_keyMap[Qt::Key(event->key())] = false;
// }

// void Realtime::mousePressEvent(QMouseEvent *event) {
//     if (event->buttons().testFlag(Qt::LeftButton)) {
//         m_mouseDown = true;
//         m_prev_mouse_pos = glm::vec2(event->position().x(), event->position().y());
//     }
// }

// void Realtime::mouseReleaseEvent(QMouseEvent *event) {
//     if (!event->buttons().testFlag(Qt::LeftButton)) {
//         m_mouseDown = false;
//     }
// }

// void Realtime::mouseMoveEvent(QMouseEvent *event) {
//     if (m_mouseDown) {
//         //New positions
//         int posX = event->position().x();
//         int posY = event->position().y();
//         //Deltas
//         int deltaX = posX - m_prev_mouse_pos.x;
//         int deltaY = posY - m_prev_mouse_pos.y;
//         m_prev_mouse_pos = glm::vec2(posX, posY);

//         if (m_mouseDown) {
//             const float sensitivity = 0.005f; // radians per pixel

//             float yaw   = -deltaX * sensitivity; // left/right
//             float pitch = -deltaY * sensitivity; // up/down

//             glm::vec3 worldUp(0.f, 1.f, 0.f); //setting up

//             // if yaw remmeber you need to rotate up because up to low nodding to rotate around world up
//             if (yaw != 0.f) {
//                 m_camLook = rotateAroundAxis(m_camLook, worldUp, yaw);
//                 m_camUp   = rotateAroundAxis(m_camUp,   worldUp, yaw);
//             }

//             // recompute right vector because yaw moved up and look around
//             glm::vec3 right = glm::normalize(glm::cross(m_camLook, m_camUp));

//             // to rotate around camera's right axis (side to side nodding)
//             if (pitch != 0.f) {
//                 m_camLook = rotateAroundAxis(m_camLook, right, pitch);
//                 m_camUp   = rotateAroundAxis(m_camUp,   right, pitch);
//             }

//             // Re-orthonormalize basis (just redoing vectors to make sure everything is in the right vector position/perpendicular)
//             m_camLook = glm::normalize(m_camLook);
//             right     = glm::normalize(glm::cross(m_camLook, m_camUp));
//             m_camUp   = glm::normalize(glm::cross(right, m_camLook));

//             m_camera.setViewMatrix(m_camPos, m_camLook, m_camUp);
//             update();
//         }
//     }
// }

// void Realtime::timerEvent(QTimerEvent *event) {
//     int elapsedms   = m_elapsedTimer.elapsed();
//     float deltaTime = elapsedms * 0.001f;
//     m_elapsedTimer.restart();

//     // Use deltaTime and m_keyMap here to move around

//     const float speed = 5.f; // world units per second

//     glm::vec3 forward = glm::normalize(m_camLook);
//     glm::vec3 right   = glm::normalize(glm::cross(forward, m_camUp));
//     glm::vec3 worldUp(0.f, 1.f, 0.f);

//     glm::vec3 moveDir(0.f);

//     // W/S: along look direction
//     if (m_keyMap[Qt::Key_W]) moveDir += forward;
//     if (m_keyMap[Qt::Key_S]) moveDir -= forward;

//     // A/D: strafe left/right
//     if (m_keyMap[Qt::Key_D]) moveDir += right;
//     if (m_keyMap[Qt::Key_A]) moveDir -= right;

//     // Space / Ctrl: vertical in world space
//     if (m_keyMap[Qt::Key_Space])   moveDir += worldUp;
//     if (m_keyMap[Qt::Key_Control]) moveDir -= worldUp;

//     if (glm::length(moveDir) > 0.f) {
//         moveDir = glm::normalize(moveDir);
//         m_camPos += moveDir * speed * deltaTime;

//         // Push updated camera to Camera object
//         m_camera.setViewMatrix(m_camPos, m_camLook, m_camUp);
//     }

//     update();


// }

// // DO NOT EDIT
// void Realtime::saveViewportImage(std::string filePath) {
//     // Make sure we have the right context and everything has been drawn
//     makeCurrent();

//     int fixedWidth = 1024;
//     int fixedHeight = 768;

//     // Create Frame Buffer
//     GLuint fbo;
//     glGenFramebuffers(1, &fbo);
//     glBindFramebuffer(GL_FRAMEBUFFER, fbo);

//     // Create a color attachment texture
//     GLuint texture;
//     glGenTextures(1, &texture);
//     glBindTexture(GL_TEXTURE_2D, texture);
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fixedWidth, fixedHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
//     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

//     // Optional: Create a depth buffer if your rendering uses depth testing
//     GLuint rbo;
//     glGenRenderbuffers(1, &rbo);
//     glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//     glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fixedWidth, fixedHeight);
//     glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

//     if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
//         std::cerr << "Error: Framebuffer is not complete!" << std::endl;
//         glBindFramebuffer(GL_FRAMEBUFFER, 0);
//         return;
//     }

//     // Render to the FBO
//     glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//     glViewport(0, 0, fixedWidth, fixedHeight);

//     // Clear and render your scene here
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//     paintGL();

//     // Read pixels from framebuffer
//     std::vector<unsigned char> pixels(fixedWidth * fixedHeight * 3);
//     glReadPixels(0, 0, fixedWidth, fixedHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

//     // Unbind the framebuffer to return to default rendering to the screen
//     glBindFramebuffer(GL_FRAMEBUFFER, 0);

//     // Convert to QImage
//     QImage image(pixels.data(), fixedWidth, fixedHeight, QImage::Format_RGB888);
//     QImage flippedImage = image.mirrored(); // Flip the image vertically

//     // Save to file using Qt
//     QString qFilePath = QString::fromStdString(filePath);
//     if (!flippedImage.save(qFilePath)) {
//         std::cerr << "Failed to save image to " << filePath << std::endl;
//     }

//     // Clean up
//     glDeleteTextures(1, &texture);
//     glDeleteRenderbuffers(1, &rbo);
//     glDeleteFramebuffers(1, &fbo);
// }

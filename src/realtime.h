#pragma once

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <QOpenGLWidget>
#include <QElapsedTimer>
#include <unordered_map>

#include "utils/sceneparser.h"
#include "utils/camera.h"
#include "utils/gbuffer.h"

class Realtime : public QOpenGLWidget {
public:
    Realtime(QWidget* parent = nullptr);
    void finish();
    void sceneChanged();
    void settingsChanged();
    void saveViewportImage(const std::string& path);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void timerEvent(QTimerEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    RenderData m_renderData;
    Camera m_camera;

    glm::vec3 m_camPos;
    glm::vec3 m_camLook;
    glm::vec3 m_camUp;

    QElapsedTimer m_elapsedTimer;
    int m_timer;

    bool m_mouseDown;
    glm::vec2 m_prevMousePos;

    std::unordered_map<int, bool> m_keyMap;

    void updateCamera(float deltaTime);

    // VAOs for shapes
    std::unordered_map<PrimitiveType, GLuint> m_shapeVAOs;
    std::unordered_map<PrimitiveType, int> m_shapeVertexCounts;

    GLuint m_defaultFBO = 2; // Default to 2 for HighDPI displays, updated in init

    // Deferred Rendering
    GLuint m_gbufferShader;  // geometry.vert/frag
    GLuint m_deferredShader; // fullscreen.vert / deferredLighting.frag

    // Fullscreen quad
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    GBuffer m_gbuffer;

    // Add these
    GLuint m_pingpongFBO[2];
    GLuint m_pingpongColorbuffers[2]; // The textures
    GLuint m_blurShader;
    GLuint m_compositeShader; // Mixes scene + bloom

    GLuint m_lightingFBO;
    GLuint m_lightingTexture;
};

// #pragma once

// // Defined before including GLEW to suppress deprecation messages on macOS
// #ifdef __APPLE__
// #define GL_SILENCE_DEPRECATION
// #endif
// #include <GL/glew.h>
// #include <glm/glm.hpp>

// #include <unordered_map>
// #include <QElapsedTimer>
// #include <QOpenGLWidget>
// #include <QTime>
// #include <QTimer>

// #include <algorithm>
// #include "cube.h"
// #include "cone.h"
// #include "sphere.h"
// #include "cylinder.h"
// #include "shaderloader.h"
// #include "camera.h"
// #include "scenedata.h"
// #include "sceneparser.h"



// class Realtime : public QOpenGLWidget
// {
// public:
//     Realtime(QWidget *parent = nullptr);
//     void finish();                                      // Called on program exit
//     void sceneChanged();
//     void settingsChanged();
//     void saveViewportImage(std::string filePath);

// public slots:
//     void tick(QTimerEvent* event);                      // Called once per tick of m_timer

// protected:
//     void initializeGL() override;                       // Called once at the start of the program
//     void paintGL() override;                            // Called whenever the OpenGL context changes or by an update() request
//     void resizeGL(int width, int height) override;      // Called when window size changes

// private:
//     void keyPressEvent(QKeyEvent *event) override;
//     void keyReleaseEvent(QKeyEvent *event) override;
//     void mousePressEvent(QMouseEvent *event) override;
//     void mouseReleaseEvent(QMouseEvent *event) override;
//     void mouseMoveEvent(QMouseEvent *event) override;
//     void timerEvent(QTimerEvent *event) override;

//     // Tick Related Variables
//     int m_timer;                                        // Stores timer which attempts to run ~60 times per second
//     QElapsedTimer m_elapsedTimer;                       // Stores timer which keeps track of actual time between frames

//     // Input Related Variables
//     bool m_mouseDown = false;                           // Stores state of left mouse button
//     glm::vec2 m_prev_mouse_pos;                         // Stores mouse position
//     std::unordered_map<Qt::Key, bool> m_keyMap;         // Stores whether keys are pressed or not


//     double m_devicePixelRatio;

//     GLuint m_shader = 0;

//     struct ShapeVAO {
//         GLuint vao;
//         GLuint vbo;
//         int vertexCount;
//     };

//     std::vector<ShapeVAO> m_shapeVAOs;


//     void loadScene();
//     void generateShapeVAOs();
//     void cleanupVAOs();
//     std::vector<float> generateShapeData(PrimitiveType type, int param1, int param2);





//     Camera m_camera;
//     glm::vec3 m_camPos  = glm::vec3(0.f, 0.f, 5.f);
//     glm::vec3 m_camLook = glm::vec3(0.f, 0.f, -1.f);
//     glm::vec3 m_camUp   = glm::vec3(0.f, 1.f, 0.f);

//     // Parsed scene data from lab 4
//     RenderData m_renderData;


// };

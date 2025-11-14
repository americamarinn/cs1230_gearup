#include "realtime.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <iostream>
#include "settings.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

// ================== Rendering the Scene!

Realtime::Realtime(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_prev_mouse_pos = glm::vec2(size().width()/2, size().height()/2);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_keyMap[Qt::Key_W]       = false;
    m_keyMap[Qt::Key_A]       = false;
    m_keyMap[Qt::Key_S]       = false;
    m_keyMap[Qt::Key_D]       = false;
    m_keyMap[Qt::Key_Control] = false;
    m_keyMap[Qt::Key_Space]   = false;

    // If you must use this function, do not edit anything above this
}

void Realtime::finish() {
    killTimer(m_timer);
    this->makeCurrent();

    // Students: anything requiring OpenGL calls when the program exits should be done here

    if (m_vbo)    glDeleteBuffers(1, &m_vbo);
    if (m_vao)    glDeleteVertexArrays(1, &m_vao);
    if (m_shader) glDeleteProgram(m_shader);

    this->doneCurrent();
}

void Realtime::updateCylinderFromSettings() {
    // Use UI values, but clamp to valid ranges
    int p1 = std::max(1, settings.shapeParameter1);   // vertical
    int p2 = std::max(3, settings.shapeParameter2);   // around

    m_cylinder.updateParams(p1, p2);
    std::vector<float> data = m_cylinder.generateShape();
    m_vertexCount = static_cast<int>(data.size() / 6);   // 3 pos + 3 normal

    std::cout << "[Realtime] updateCylinderFromSettings: p1="
              << p1 << " p2=" << p2
              << " verts=" << m_vertexCount << std::endl;

    // Upload new data into existing VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 data.size() * sizeof(float),
                 data.data(),
                 GL_STATIC_DRAW);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void Realtime::initTestShape() {
    // Compile & link shader program
    m_shader = ShaderLoader::createShaderProgram(
        ":/resources/shaders/default.vert",
        ":/resources/shaders/default.frag"
        );
    std::cout << "[Realtime] shader program = " << m_shader << std::endl;

    // Generate CPU mesh from your Cylinder
    int p1 = std::max(1, settings.shapeParameter1);
    int p2 = std::max(3, settings.shapeParameter2);

    m_cylinder.updateParams(p1, p2);
    std::vector<float> data = m_cylinder.generateShape();
    m_vertexCount = static_cast<int>(data.size() / 6); // 3 pos + 3 normal

    std::cout << "[Realtime] cylinder vertices = " << m_vertexCount << std::endl;

    // Create VAO & VBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);



    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    updateCylinderFromSettings();

    // layout(location = 0) vec3 aPos;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float),
                          (void*)0);

    // layout(location = 1) vec3 aNor;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}




void Realtime::initializeGL() {
    m_devicePixelRatio = this->devicePixelRatio();

    m_timer = startTimer(1000/60);
    m_elapsedTimer.start();

    // Initializing GL.
    // GLEW (GL Extension Wrangler) provides access to OpenGL functions.
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Error while initializing GL: " << glewGetErrorString(err) << std::endl;
    }
    std::cout << "Initialized GL: Version " << glewGetString(GLEW_VERSION) << std::endl;

    // Allows OpenGL to draw objects appropriately on top of one another
    glEnable(GL_DEPTH_TEST);
    // Tells OpenGL to only draw the front face
    glEnable(GL_CULL_FACE);
    // Tells OpenGL how big the screen is
    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Students: anything requiring OpenGL calls when the program starts should be done here

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    initTestShape();

    // Simple default: eye at (0, 0, 3) looking at origin
    m_camera.setViewMatrix(
        glm::vec3(0.f, 0.f, 3.f),   // position
        glm::vec3(0.f, 0.f, -1.f),  // look direction
        glm::vec3(0.f, 1.f, 0.f)    // up
        );

    // Field of view ~45 degrees (in radians)
    const float fovY = 45.f * 3.1415926535f / 180.f;

    float aspect = float(size().width()) / float(size().height());
    m_camera.setProjectionMatrix(
        aspect,
        settings.nearPlane,
        settings.farPlane,
        fovY
        );
}



void Realtime::paintGL() {
    // Students: anything requiring OpenGL calls every frame should be done here
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!m_shader || !m_vao || m_vertexCount == 0) {
        return;
    }

    glUseProgram(m_shader);
    glBindVertexArray(m_vao);

    // For now, model = identity (cylinder at origin)
    glm::mat4 model(1.f);
    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 proj = m_camera.getProjMatrix();

    GLint modelLoc = glGetUniformLocation(m_shader, "model");
    GLint viewLoc  = glGetUniformLocation(m_shader, "view");
    GLint projLoc  = glGetUniformLocation(m_shader, "proj");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(viewLoc,  1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc,  1, GL_FALSE, &proj[0][0]);

    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);

    glBindVertexArray(0);
    glUseProgram(0);
}

void Realtime::resizeGL(int w, int h) {
    // Tells OpenGL how big the screen is
    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Students: anything requiring OpenGL calls when the program starts should be done here

    // Keep camera projection in sync with window size
    const float fovY = 45.f * 3.1415926535f / 180.f;
    float aspect = (h > 0) ? float(w) / float(h) : 1.f;

    m_camera.setProjectionMatrix(
        aspect,
        settings.nearPlane,
        settings.farPlane,
        fovY
        );
}

void Realtime::sceneChanged() {

    // Called when the user clicks "Upload Scene File" and picks a JSON.
    makeCurrent();  // we might later do GL work here

    if (settings.sceneFilePath.empty()) {
        std::cout << "[Realtime] sceneChanged: no scene file selected\n";
        doneCurrent();
        return;
    }

    RenderData newData;
    bool ok = SceneParser::parse(settings.sceneFilePath, newData);
    if (!ok) {
        std::cerr << "[Realtime] Failed to parse scene file: "
                  << settings.sceneFilePath << std::endl;
        doneCurrent();
        return;
    }

    // Store it
    m_renderData = std::move(newData);

    std::cout << "[Realtime] Scene parsed:\n"
              << "  shapes = " << m_renderData.shapes.size() << "\n"
              << "  lights = " << m_renderData.lights.size() << std::endl;

    // ---- Update camera from scene file ----
    const SceneCameraData &cam = m_renderData.cameraData;

    // SceneCameraData stores vec4; we only need xyz
    glm::vec3 pos  = glm::vec3(cam.pos);
    glm::vec3 look = glm::vec3(cam.look);
    glm::vec3 up   = glm::vec3(cam.up);

    m_camera.setViewMatrix(pos, look, up);

    float aspect =
        (size().height() > 0)
            ? float(size().width()) / float(size().height())
            : 1.f;

    // cam.heightAngle is already in radians
    m_camera.setProjectionMatrix(
        aspect,
        settings.nearPlane,
        settings.farPlane,
        cam.heightAngle
        );

    doneCurrent();

    update(); // triggers paintGL
}

void Realtime::settingsChanged() {

    makeCurrent();

    if (m_vbo != 0) {
        updateCylinderFromSettings();
    }

    update(); // asks for a PaintGL() call to occur
}

// ================== Camera Movement!

void Realtime::keyPressEvent(QKeyEvent *event) {
    m_keyMap[Qt::Key(event->key())] = true;
}

void Realtime::keyReleaseEvent(QKeyEvent *event) {
    m_keyMap[Qt::Key(event->key())] = false;
}

void Realtime::mousePressEvent(QMouseEvent *event) {
    if (event->buttons().testFlag(Qt::LeftButton)) {
        m_mouseDown = true;
        m_prev_mouse_pos = glm::vec2(event->position().x(), event->position().y());
    }
}

void Realtime::mouseReleaseEvent(QMouseEvent *event) {
    if (!event->buttons().testFlag(Qt::LeftButton)) {
        m_mouseDown = false;
    }
}

void Realtime::mouseMoveEvent(QMouseEvent *event) {
    if (m_mouseDown) {
        int posX = event->position().x();
        int posY = event->position().y();
        int deltaX = posX - m_prev_mouse_pos.x;
        int deltaY = posY - m_prev_mouse_pos.y;
        m_prev_mouse_pos = glm::vec2(posX, posY);

        // Use deltaX and deltaY here to rotate

        update(); // asks for a PaintGL() call to occur
    }
}

void Realtime::timerEvent(QTimerEvent *event) {
    int elapsedms   = m_elapsedTimer.elapsed();
    float deltaTime = elapsedms * 0.001f;
    m_elapsedTimer.restart();

    // Use deltaTime and m_keyMap here to move around

    update(); // asks for a PaintGL() call to occur
}

// DO NOT EDIT
void Realtime::saveViewportImage(std::string filePath) {
    // Make sure we have the right context and everything has been drawn
    makeCurrent();

    int fixedWidth = 1024;
    int fixedHeight = 768;

    // Create Frame Buffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create a color attachment texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fixedWidth, fixedHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Optional: Create a depth buffer if your rendering uses depth testing
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fixedWidth, fixedHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error: Framebuffer is not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // Render to the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, fixedWidth, fixedHeight);

    // Clear and render your scene here
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    paintGL();

    // Read pixels from framebuffer
    std::vector<unsigned char> pixels(fixedWidth * fixedHeight * 3);
    glReadPixels(0, 0, fixedWidth, fixedHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Unbind the framebuffer to return to default rendering to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Convert to QImage
    QImage image(pixels.data(), fixedWidth, fixedHeight, QImage::Format_RGB888);
    QImage flippedImage = image.mirrored(); // Flip the image vertically

    // Save to file using Qt
    QString qFilePath = QString::fromStdString(filePath);
    if (!flippedImage.save(qFilePath)) {
        std::cerr << "Failed to save image to " << filePath << std::endl;
    }

    // Clean up
    glDeleteTextures(1, &texture);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);
}

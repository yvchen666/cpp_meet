#include "qtclient/yuv_renderer.h"
#include <QMetaObject>

// BT.601 YUV420P -> RGB fragment shader
const char* YuvRenderer::VERTEX_SHADER = R"(
    #version 330 core
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* YuvRenderer::FRAGMENT_SHADER = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D texY;
    uniform sampler2D texU;
    uniform sampler2D texV;
    void main() {
        float y = texture(texY, TexCoord).r;
        float u = texture(texU, TexCoord).r;
        float v = texture(texV, TexCoord).r;
        vec3 yuv = vec3(y - 0.0625, u - 0.5, v - 0.5);
        float r = yuv.x * 1.164 + yuv.z * 1.596;
        float g = yuv.x * 1.164 - yuv.y * 0.391 - yuv.z * 0.813;
        float b = yuv.x * 1.164 + yuv.y * 2.018;
        FragColor = vec4(clamp(r, 0.0, 1.0),
                         clamp(g, 0.0, 1.0),
                         clamp(b, 0.0, 1.0),
                         1.0);
    }
)";

YuvRenderer::YuvRenderer(QWidget* parent)
    : QOpenGLWidget(parent)
{}

YuvRenderer::~YuvRenderer() {
    makeCurrent();
    if (textures_[0]) glDeleteTextures(3, textures_);
    delete program_;
    doneCurrent();
}

void YuvRenderer::initializeGL() {
    initializeOpenGLFunctions();

    program_ = new QOpenGLShaderProgram(this);
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex,   VERTEX_SHADER);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAGMENT_SHADER);
    program_->link();

    // 创建 3 个纹理（Y, U, V），格式 GL_RED
    glGenTextures(3, textures_);
    for (int i = 0; i < 3; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void YuvRenderer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void YuvRenderer::paintGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!has_frame_ || frame_width_ <= 0 || frame_height_ <= 0) return;

    // 上传 Y 纹理
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width_, frame_height_,
                 0, GL_RED, GL_UNSIGNED_BYTE,
                 reinterpret_cast<const uint8_t*>(y_plane_.constData()));

    // 上传 U 纹理（宽高各半）
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width_ / 2, frame_height_ / 2,
                 0, GL_RED, GL_UNSIGNED_BYTE,
                 reinterpret_cast<const uint8_t*>(u_plane_.constData()));

    // 上传 V 纹理（宽高各半）
    glBindTexture(GL_TEXTURE_2D, textures_[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width_ / 2, frame_height_ / 2,
                 0, GL_RED, GL_UNSIGNED_BYTE,
                 reinterpret_cast<const uint8_t*>(v_plane_.constData()));

    program_->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    program_->setUniformValue("texY", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    program_->setUniformValue("texU", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures_[2]);
    program_->setUniformValue("texV", 2);

    // 全屏四边形（NDC 坐标）
    static const float vertices[] = {
        // pos        texcoord
        -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
    };

    program_->enableAttributeArray(0);
    program_->setAttributeArray(0, GL_FLOAT, vertices,       2, 4 * sizeof(float));
    program_->enableAttributeArray(1);
    program_->setAttributeArray(1, GL_FLOAT, vertices + 2,  2, 4 * sizeof(float));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    program_->disableAttributeArray(0);
    program_->disableAttributeArray(1);
    program_->release();
}

void YuvRenderer::updateFrame(int width, int height,
                               QByteArray y, QByteArray u, QByteArray v)
{
    QMetaObject::invokeMethod(this, [this, width, height,
                                     y = std::move(y),
                                     u = std::move(u),
                                     v = std::move(v)]() mutable {
        frame_width_  = width;
        frame_height_ = height;
        y_plane_ = std::move(y);
        u_plane_ = std::move(u);
        v_plane_ = std::move(v);
        has_frame_ = true;
        update();
    }, Qt::QueuedConnection);
}

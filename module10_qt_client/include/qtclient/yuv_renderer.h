#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <vector>

// YUV420P OpenGL 渲染器
// 上传三张 GL_RED 纹理，fragment shader 做 BT.601 矩阵转换
class YuvRenderer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit YuvRenderer(QWidget* parent = nullptr);
    ~YuvRenderer() override;

public slots:
    void updateFrame(int width, int height, QByteArray y, QByteArray u, QByteArray v);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram* program_{nullptr};
    GLuint textures_[3]{0, 0, 0};  // Y, U, V
    int frame_width_{0}, frame_height_{0};
    QByteArray y_plane_, u_plane_, v_plane_;
    bool has_frame_{false};

    static const char* VERTEX_SHADER;
    static const char* FRAGMENT_SHADER;
};

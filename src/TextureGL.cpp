#include "TextureGL.h"
#include "glad.h" // Opengl function loader
#include <cassert>
#include <iostream>

TextureGL::TextureGL(int width, int height, TextureGLType datatype, const void* data)
    : width(width)
    , height(height)
{
    glGenTextures(1, &textureID);

    glBindTexture(GL_TEXTURE_2D, textureID);
    switch (datatype) {
    case TextureGLType::RGB_32F: {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);
    } break;
    case TextureGLType::RGBA_32F: {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, data);
    } break;
    default:
        break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int error = glGetError();
    if (error)
        std::cerr << error << std::endl;

    glBindTexture(GL_TEXTURE_2D, 0);
}

TextureGL::TextureGL(TextureGL&& other)
{
    other.textureID = this->textureID;
    other.width = this->width;
    other.height = this->height;
}

int TextureGL::getWidth()
{
    return width;
}

int TextureGL::getHeight()
{
    return height;
}

void TextureGL::bind()
{
    glBindTexture(GL_TEXTURE_2D, textureID);
}

TextureGL::~TextureGL()
{
    glDeleteTextures(1, &textureID);
}

#include "BVHBuilder.h"
#include "ModelLoader.h"
#include "SDLHelper.h"
#include "ShaderProgram.h"
#include "TextureGL.h"
#include "Utils.h"
#include "glad.h" // Opengl function loader
#include <assert.h>
#include <fwd.hpp> //GLM
#include <gtc/matrix_transform.hpp>
#include <iostream>
#include <map>

using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using std::vector;

constexpr int TEXTURE_WIDTH = 256;

#define LOG(x) std::cout << x << std::endl

constexpr int WinWidth = 1920;
constexpr int WinHeight = 1080;

std::map<int, bool> buttinInputKeys; // keyboard key
float yaw = -90.0f; // for cam rotate
float pitch = 00.0f; // for cam rotate

void loadGeometry(BVHBuilder& bvh, std::string const& path, Model3D& model)
{
    vector<float> vertex;
    vector<float> normal;
    vector<float> uv;

    ModelLoader::Obj(path, vertex, normal, uv);
    model = ModelLoader::toSingleMeshArray(vertex, normal, uv);
    bvh.build(model);

    //    for (int i = 0; i < bvh.getNodes().size(); ++i) {
    //        const auto& n = bvh.getNodes()[i];
    //        std::cout << "Node: " << i
    //                  << "\t leftChild: " << n.leftChild
    //                  << "\t rightChild: " << n.rightChild
    //                  << std::endl;
    //    }

    //    assert(false);

    const int floatsPerPixel = 3;

    int pixelCount = vertex.size() / floatsPerPixel;
    int textureHeight = Utils::powerOfTwo(((pixelCount - 1) / TEXTURE_WIDTH) + 1);
    pixelCount = TEXTURE_WIDTH * textureHeight;

    vertex.resize(pixelCount * floatsPerPixel, 0.0);
}

TextureGL createIndexTexture(const Model3D& model)
{
    const int floatsPerPixel = 4;
    const auto format = TextureGLType::RGBA_32F;

    int numFloatsInIndex = 4;
    int numOfFloatsInIndexArray = model.triangles.size() * numFloatsInIndex; // triangle size is 3, align to 4
    int indexPixelCount = numOfFloatsInIndexArray / floatsPerPixel;

    int indexTextureHeight = ((indexPixelCount - 1) / TEXTURE_WIDTH) + 1;
    indexTextureHeight = Utils::powerOfTwo(indexTextureHeight);

    indexPixelCount = TEXTURE_WIDTH * indexTextureHeight;
    numOfFloatsInIndexArray = indexPixelCount * floatsPerPixel;

    std::vector<float> indexBuffer;
    indexBuffer.resize(numOfFloatsInIndexArray, 0);
    for (int i = 0; i < model.triangles.size(); ++i) {
        const auto& t = model.triangles[i];
        indexBuffer[i * numFloatsInIndex + 0] = (float)t[0];
        indexBuffer[i * numFloatsInIndex + 1] = (float)t[1];
        indexBuffer[i * numFloatsInIndex + 2] = (float)t[2];
        indexBuffer[i * numFloatsInIndex + 3] = 0.f;
    }

    return TextureGL(TEXTURE_WIDTH, indexTextureHeight, format, indexBuffer.data());
}

TextureGL createVertexArrayTexture(const Model3D& model)
{
    const int floatsPerPixel = 4;
    const auto format = TextureGLType::RGBA_32F;

    int numFloatsInVertex = 8;
    int numOfFloatsInVertexArray = model.vertices.size() * numFloatsInVertex;
    int vertexPixelCount = numOfFloatsInVertexArray / floatsPerPixel;

    int vertexTextureHeight = ((vertexPixelCount - 1) / TEXTURE_WIDTH) + 1;
    vertexTextureHeight = Utils::powerOfTwo(vertexTextureHeight);

    vertexPixelCount = TEXTURE_WIDTH * vertexTextureHeight;
    numOfFloatsInVertexArray = vertexPixelCount * floatsPerPixel;

    std::vector<float> vertexArrayBuffer;
    vertexArrayBuffer.resize(numOfFloatsInVertexArray, 0);
    for (int i = 0; i < model.vertices.size(); ++i) {
        const auto& v = model.vertices[i];
        // first pixel
        vertexArrayBuffer[i * numFloatsInVertex + 0] = v.position.x;
        vertexArrayBuffer[i * numFloatsInVertex + 1] = v.position.y;
        vertexArrayBuffer[i * numFloatsInVertex + 2] = v.position.z;
        vertexArrayBuffer[i * numFloatsInVertex + 3] = v.normal.x;

        // second pixel
        vertexArrayBuffer[i * numFloatsInVertex + 4] = v.normal.y;
        vertexArrayBuffer[i * numFloatsInVertex + 5] = v.normal.z;
        vertexArrayBuffer[i * numFloatsInVertex + 6] = v.uv.x;
        vertexArrayBuffer[i * numFloatsInVertex + 7] = v.uv.y;
    }


    return TextureGL(TEXTURE_WIDTH, vertexTextureHeight, format, vertexArrayBuffer.data());
}

TextureGL BVHNodesToTexture(BVHBuilder& bvh)
{
    float const* texNodeData = (float*)(bvh.bvhToTexture());
    int texWidthNode = bvh.getTextureSideSize();
    return TextureGL(texWidthNode, texWidthNode, TextureGLType::RGBA_32F, texNodeData);
}

// FPS Camera rotate
void updateMatrix(glm::mat3& viewToWorld)
{
    mat4 matPitch = mat4(1.0);
    mat4 matYaw = mat4(1.0);

    matPitch = glm::rotate(matPitch, glm::radians(pitch), vec3(1, 0, 0));
    matYaw = glm::rotate(matYaw, glm::radians(yaw), vec3(0, 1, 0));

    mat4 rotateMatrix = matYaw * matPitch;
    viewToWorld = mat3(rotateMatrix);
}

// FPS Camera move
void cameraMove(vec3& location, glm::mat3 const& viewToWorld)
{
    float speed = 0.2f;
    if (buttinInputKeys[SDLK_w])
        location += viewToWorld * vec3(0, 0, 1) * speed;

    if (buttinInputKeys[SDLK_s])
        location += viewToWorld * vec3(0, 0, -1) * speed;

    if (buttinInputKeys[SDLK_a])
        location += viewToWorld * vec3(-1, 0, 0) * speed;

    if (buttinInputKeys[SDLK_d])
        location += viewToWorld * vec3(1, 0, 0) * speed;

    if (buttinInputKeys[SDLK_q])
        location += viewToWorld * vec3(0, 1, 0) * speed;

    if (buttinInputKeys[SDLK_e])
        location += viewToWorld * vec3(0, -1, 0) * speed;
}

int main(int ArgCount, char** Args)
{
    // Set Opengl Specification
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // SDL window and Opengl
    SDLWindowPtr window(SDL_CreateWindow("OpenGL ray casting", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WinWidth, WinHeight, SDL_WINDOW_OPENGL));
    if (!window) {
        std::cerr << "Failed create window" << std::endl;
        return -1;
    }

    SDLGLContextPtr context(SDL_GL_CreateContext(window));
    if (!context) {
        std::cerr << "Failed to create OpenGL context" << std::endl;
        return -1;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }
    SDL_GL_SetSwapInterval(0);

    std::cout << "OpenGL version " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // GL 3.3, you must have though 1 VAO for correct work vertex shader
    uint32_t VAO;
    glGenVertexArrays(1, &VAO);

    // Load geometry, build BVH && and load data to texture
    BVHBuilder* bvh = new BVHBuilder(); // Big object

    Model3D model;
    loadGeometry(*bvh, "models/BullPlane.obj", model);
    TextureGL texNode = BVHNodesToTexture(*bvh);
    TextureGL texIndex = createIndexTexture(model);
    TextureGL texVertArray = createVertexArrayTexture(model);
    ShaderProgram shaderProgram("shaders/vertex.vert", "shaders/raytracing.frag");

    // Variable for camera
    vec3 location = vec3(11, 0.01, -0.501);
    mat3 viewToWorld = mat3(1.0f);

    // Add value  in map
    buttinInputKeys[SDLK_w] = false;
    buttinInputKeys[SDLK_s] = false;
    buttinInputKeys[SDLK_a] = false;
    buttinInputKeys[SDLK_d] = false;
    buttinInputKeys[SDLK_q] = false;
    buttinInputKeys[SDLK_e] = false;

    // Event loop
    SDL_Event Event;
    auto keyIsInside = [&Event] { return buttinInputKeys.count(Event.key.keysym.sym); }; // check key inside in buttinInputKeys

    float dtSmooth = 0.f;
    while (true) {
        while (SDL_PollEvent(&Event)) {
            if (Event.type == SDL_QUIT)
                return 0;

            if (Event.type == SDL_MOUSEMOTION) {
                pitch += Event.motion.yrel * 0.08;
                yaw += Event.motion.xrel * 0.08;
            }

            if (Event.type == SDL_KEYDOWN && keyIsInside())
                buttinInputKeys[Event.key.keysym.sym] = true;

            if (Event.type == SDL_KEYUP && keyIsInside())
                buttinInputKeys[Event.key.keysym.sym] = false;

            if (Event.type == SDL_KEYDOWN && Event.key.keysym.sym == SDLK_ESCAPE)
                return 0;
        }

        cameraMove(location, viewToWorld);
        updateMatrix(viewToWorld);

        // Render/Draw
        // Clear the colorbuffer

        glViewport(0, 0, WinWidth, WinHeight);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shaderProgram.bind();
        glBindVertexArray(VAO);

        shaderProgram.setMatrix3x3("viewToWorld", viewToWorld);
        shaderProgram.setVec3("location", location);
        shaderProgram.setVec2("screeResolution", vec2(WinWidth, WinHeight));

        shaderProgram.setTextureAI("texNode", texNode);
        shaderProgram.setInt2("texNodeSize", texNode.getWidth(), texNode.getHeight());

        shaderProgram.setTextureAI("texIndices", texIndex);
        shaderProgram.setInt2("texIndicesSize", texIndex.getWidth(), texIndex.getHeight());

        shaderProgram.setTextureAI("texVertArray", texVertArray);
        shaderProgram.setInt2("texVertArraySize", texVertArray.getWidth(), texVertArray.getHeight());

        uint64_t currentTimeStamp = SDL_GetPerformanceCounter();

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);

        float dt = (float)((SDL_GetPerformanceCounter() - currentTimeStamp)
            / (float)SDL_GetPerformanceFrequency());
        dtSmooth = dtSmooth * 0.9f + dt * 0.1f;

        SDL_SetWindowTitle(window.get(), std::to_string(dtSmooth * 1000).c_str());
    }
}

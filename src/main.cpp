#include "BVHBuilder.h"
#include "ModelLoader.h"
#include "SDLHelper.h"
#include "ShaderProgram.h"
#include "TextureGL.h"
#include "Utils.h"
#include "glad.h" // Opengl function loader
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <fwd.hpp> //GLM
#include <gtc/matrix_transform.hpp>
#include <iostream>
#include <map>
#include <sstream>

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
}

TextureGL createGeometryTexture(const BVHBuilder& bvh, const Model3D& model)
{
    constexpr int floatsPerPixel = 4;
    constexpr auto format = TextureGLType::RGBA_32F;

    constexpr int nFloatsInNode = 8;
    constexpr int nFloatsInIndex = 4;
    constexpr int nFloatsInVertex = 8;
    constexpr int nPixelPerNode = nFloatsInNode / floatsPerPixel;
    constexpr int nPixelPerVertex = nFloatsInVertex / floatsPerPixel;

    // calculate index buffer size
    int numOfFloatsInNodeArray = bvh.getNodes().size() * nFloatsInNode;
    int nodePixelCount = numOfFloatsInNodeArray / floatsPerPixel;

    // calculate index buffer size
    int numOfFloatsInIndexArray = model.triangles.size() * nFloatsInIndex;
    int indexPixelCount = numOfFloatsInIndexArray / floatsPerPixel;

    // calculate vertex buffer size
    int numOfFloatsInVertexArray = model.vertices.size() * nFloatsInVertex;
    int vertexPixelCount = numOfFloatsInVertexArray / floatsPerPixel;

    int overallPixelCount = nodePixelCount + indexPixelCount + vertexPixelCount;
    int textureHeight = ((overallPixelCount - 1) / TEXTURE_WIDTH) + 1;
    textureHeight = Utils::powerOfTwo(textureHeight);

    int floatOffset = 0;

    std::vector<float> buffer;
    buffer.resize(textureHeight * TEXTURE_WIDTH * floatsPerPixel, 0);

    const int nodeIndexPixelOffset = nodePixelCount;

    for (int i = 0; i < bvh.getNodes().size(); ++i) {
        const auto& n = bvh.getNodes()[i];

        int leftChildIndex = (n.leftChild <= 0)
            ? n.leftChild - nodeIndexPixelOffset // if triangle
            : n.leftChild * nPixelPerNode; //  if node

        int rightChildIndex = (n.rightChild <= 0)
            ? n.rightChild - nodeIndexPixelOffset
            : n.rightChild * nPixelPerNode;

        // first pixel
        buffer[i * nFloatsInNode + 0] = reinterpret_cast<float&>(leftChildIndex);
        buffer[i * nFloatsInNode + 1] = reinterpret_cast<float&>(rightChildIndex);
        buffer[i * nFloatsInNode + 2] = n.aabb.getMin().x;
        buffer[i * nFloatsInNode + 3] = n.aabb.getMin().y;

        // second pixel
        buffer[i * nFloatsInNode + 4] = n.aabb.getMin().z;
        buffer[i * nFloatsInNode + 5] = n.aabb.getMax().x;
        buffer[i * nFloatsInNode + 6] = n.aabb.getMax().y;
        buffer[i * nFloatsInNode + 7] = n.aabb.getMax().z;
    }
    floatOffset += numOfFloatsInNodeArray;

    const int triIndexPixelOffset = nodePixelCount + indexPixelCount;

    for (int i = 0; i < model.triangles.size(); ++i) {
        const auto& t = model.triangles[i];

        int t0 = t[0] * nPixelPerVertex + triIndexPixelOffset;
        int t1 = t[1] * nPixelPerVertex + triIndexPixelOffset;
        int t2 = t[2] * nPixelPerVertex + triIndexPixelOffset;
        buffer[floatOffset + i * nFloatsInIndex + 0] = reinterpret_cast<float&>(t0);
        buffer[floatOffset + i * nFloatsInIndex + 1] = reinterpret_cast<float&>(t1);
        buffer[floatOffset + i * nFloatsInIndex + 2] = reinterpret_cast<float&>(t2);
        buffer[floatOffset + i * nFloatsInIndex + 3] = 0;
    }
    floatOffset += numOfFloatsInIndexArray;

    for (int i = 0; i < model.vertices.size(); ++i) {
        const auto& v = model.vertices[i];

        // first pixel
        buffer[floatOffset + i * nFloatsInVertex + 0] = v.position.x;
        buffer[floatOffset + i * nFloatsInVertex + 1] = v.position.y;
        buffer[floatOffset + i * nFloatsInVertex + 2] = v.position.z;
        buffer[floatOffset + i * nFloatsInVertex + 3] = v.normal.x;

        // second pixel
        buffer[floatOffset + i * nFloatsInVertex + 4] = v.normal.y;
        buffer[floatOffset + i * nFloatsInVertex + 5] = v.normal.z;
        buffer[floatOffset + i * nFloatsInVertex + 6] = v.uv.x;
        buffer[floatOffset + i * nFloatsInVertex + 7] = v.uv.y;
    }
    LOG("Node pixel count: " << nodePixelCount
                             << ", Index pixel count: " << indexPixelCount
                             << ", Vertex pixel count: " << vertexPixelCount);
    LOG("TextureResolution: " << TEXTURE_WIDTH << "x" << textureHeight);

    return TextureGL(TEXTURE_WIDTH, textureHeight, format, buffer.data());
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
    // loadGeometry(*bvh, "models/BullPlane.obj", model);
    loadGeometry(*bvh, "models/stanford_dragon.obj", model);
    // loadGeometry(*bvh, "models/susanne_lowpoly.obj", model);
    TextureGL texAllGeometry = createGeometryTexture(*bvh, model);
    // TextureGL texVertArray = createVertexArrayTexture(model);
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

        shaderProgram.setTextureAI("texGeometry", texAllGeometry);
        shaderProgram.setInt2("texGeometrySize", texAllGeometry.getWidth(), texAllGeometry.getHeight());

        // shaderProgram.setTextureAI("texVertArray", texVertArray);
        // shaderProgram.setInt2("texVertArraySize", texVertArray.getWidth(), texVertArray.getHeight());

        uint64_t currentTimeStamp = SDL_GetPerformanceCounter();

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);

        float dt = (float)((SDL_GetPerformanceCounter() - currentTimeStamp)
            / (float)SDL_GetPerformanceFrequency());
        dtSmooth = dtSmooth * 0.9f + dt * 0.1f;

        assert(dt < 1.f && "Very slow frame, maybe something goes wrong");

        SDL_SetWindowTitle(window.get(), std::to_string(dtSmooth * 1000).c_str());
    }
}

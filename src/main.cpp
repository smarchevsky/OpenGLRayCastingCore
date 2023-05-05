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

constexpr int WinWidth = 2000;
constexpr int WinHeight = 1000;

std::map<int, bool> buttinInputKeys; // keyboard key
float yaw = 0.0f; // for cam rotate
float pitch = 0.0f; // for cam rotate

TextureGL loadGeometry(BVHBuilder& bvh, std::string const& path, Model3D& outModel)
{
    vector<float> vertex;
    vector<float> normal;
    vector<float> uv;

    ModelLoader::Obj(path, vertex, normal, uv);
    outModel = ModelLoader::toSingleMeshArray(vertex, normal, uv);
    bvh.build(vertex);

    //    for (int i = 0; i < bvh.getNodes().size(); ++i) {
    //        const auto& n = bvh.getNodes()[i];
    //        std::cout << "Node: " << i
    //                  << "\t leftChild: " << n.leftChild
    //                  << "\t rightChild: " << n.rightChild
    //                  << std::endl;
    //    }

    //    assert(false);

    uint32_t vertexCount = vertex.size() / 3; // 3 vertex component x,y,z
    int sqrtVertexCount = ceil(sqrt(vertexCount)); // for sqrt demension
    int texWidthPos = Utils::powerOfTwo(sqrtVertexCount); // texture demension sqrt
    vertex.resize(texWidthPos * texWidthPos * 3, 0.0); // for pack x,y,z to  r,g,b

    return TextureGL(texWidthPos, texWidthPos, TextureGLType::RGB_32F, vertex.data());
}

TextureGL BVHNodesToTexture(BVHBuilder& bvh)
{
    float const* texNodeData = (float*)(bvh.bvhToTexture());
    int texWidthNode = bvh.getTextureSideSize();
    return TextureGL(texWidthNode, texWidthNode, TextureGLType::RGB_32F, texNodeData);
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
    float speed = 1;
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
    TextureGL texPos = loadGeometry(*bvh, "models/BullPlane.obj", model);
    TextureGL texNode = BVHNodesToTexture(*bvh);
    ShaderProgram shaderProgram("shaders/vertex.vert", "shaders/raytracing.frag");

    // Variable for camera
    vec3 location = vec3(0, 0.1, -20);
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

    float dtSmooth = 0.00001f;
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

        shaderProgram.setTextureAI("texPosition", texPos);
        shaderProgram.setTextureAI("texNode", texNode);
        shaderProgram.setMatrix3x3("viewToWorld", viewToWorld);
        shaderProgram.setVec3("location", location);
        shaderProgram.setVec2("screeResolution", vec2(WinWidth, WinHeight));
        shaderProgram.setInt("bvhWidth", texNode.getWidth());
        shaderProgram.setInt("texPosWidth", texPos.getWidth());

        uint64_t currentTimeStamp = SDL_GetPerformanceCounter();

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);

        float dt = (float)((SDL_GetPerformanceCounter() - currentTimeStamp)
            / (float)SDL_GetPerformanceFrequency());
        dtSmooth = dtSmooth * 0.9f + dt * 0.1f;

        SDL_SetWindowTitle(window.get(), std::to_string(dtSmooth * 1000).c_str());
    }
}

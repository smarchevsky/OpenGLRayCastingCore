#include "ModelLoader.h"
#include "Utils.h"
#include <fstream>
#include <iostream>

#define GLM_EXT_INCLUDED
#include <glm/gtx/string_cast.hpp>

#define LOG(x) std::cout << x << std::endl
void ModelLoader::Obj(std::string const& filePath, std::vector<float>& rawVertex, std::vector<float>& rawNormal, std::vector<float>& rawUV)
{
    rawVertex.clear();
    rawNormal.clear();
    rawUV.clear();

    std::vector<float> tempVertex;
    std::vector<float> tempNormal;
    std::vector<float> tempuv;

    auto setData = [](int startIndex, int count, std::vector<float>& sourceBuffer, std::vector<float>& forBuffer) {
        int vertexIndex = startIndex - 1;
        float* pVertex = &sourceBuffer[vertexIndex * count];
        forBuffer.push_back(pVertex[0]);
        forBuffer.push_back(pVertex[1]);
        forBuffer.push_back(pVertex[2]);
    };

    std::ifstream stream(Utils::resourceDir + filePath);
    if (!stream.is_open()) {
        std::cerr << "error load file " + filePath << std::endl;
        return;
    }

    float x, y, z;
    int v1, t1, n1, v2, t2, n2, v3, t3, n3;
    std::string s;

    while (getline(stream, s)) {
        if (sscanf(s.c_str(), "v %f %f %f", &x, &y, &z)) {
            tempVertex.push_back(x);
            tempVertex.push_back(y);
            tempVertex.push_back(z);
        }

        if (sscanf(s.c_str(), "vt %f %f", &x, &y)) {
            tempuv.push_back(x);
            tempuv.push_back(y);
        }

        if (sscanf(s.c_str(), "vn %f %f %f", &x, &y, &z)) {
            tempNormal.push_back(x);
            tempNormal.push_back(y);
            tempNormal.push_back(z);
        }

        if (sscanf(s.c_str(), "f %i/%i/%i %i/%i/%i %i/%i/%i", &v1, &t1, &n1, &v2, &t2, &n2, &v3, &t3, &n3)) {
            setData(v1, 3, tempVertex, rawVertex);
            setData(v2, 3, tempVertex, rawVertex);
            setData(v3, 3, tempVertex, rawVertex);

            setData(t1, 2, tempuv, rawUV);
            setData(t2, 2, tempuv, rawUV);
            setData(t3, 2, tempuv, rawUV);

            setData(n1, 3, tempNormal, rawNormal);
            setData(n2, 3, tempNormal, rawNormal);
            setData(n3, 3, tempNormal, rawNormal);
        }
    }
}

Model3D ModelLoader::toSingleMeshArray(
    const std::vector<float>& rawVertex,
    const std::vector<float>& rawNormal,
    const std::vector<float>& rawUV)
{
    std::vector<glm::vec3> positions;
    positions.resize(rawVertex.size() / 3);
    for (int i = 0; i < positions.size(); ++i) {
        positions[i] = { rawVertex[i * 3], rawVertex[i * 3 + 1], rawVertex[i * 3 + 2] };
    }

    std::vector<glm::vec3> normals;
    normals.resize(rawNormal.size() / 3);
    for (int i = 0; i < normals.size(); ++i) {
        normals[i] = { rawNormal[i * 3], rawNormal[i * 3 + 1], rawNormal[i * 3 + 2] };
    }

    std::vector<glm::vec2> uvs;
    uvs.resize(rawUV.size() / 3);
    for (int i = 0; i < uvs.size(); ++i) {
        uvs[i] = { rawUV[i * 3], rawUV[i * 3 + 1] };
    }

    normals.resize(positions.size(), glm::vec3(0, 0, 1));
    uvs.resize(positions.size(), glm::vec2(0, 0));

    size_t indexOffset {};
    Model3D resultModel;

    std::unordered_map<Vertex, int> pointHashMap;
    for (int i = 0; i < positions.size(); i += 3) {

        glm::ivec3 triangle(0);
        for (int j = 0; j < 3; ++j) {
            Vertex vertex { positions[i + j], normals[i + j], uvs[i + j] };
            const auto& foundCell = pointHashMap.find(vertex);

            if (foundCell != pointHashMap.end()) { // if found
                triangle[j] = foundCell->second;
            } else {
                pointHashMap.insert({ vertex, indexOffset });
                triangle[j] = indexOffset;
                indexOffset++;
            }
        }

        resultModel.triangles.push_back(triangle);
    }

    resultModel.vertices.resize(pointHashMap.size());

    for (const auto& p : pointHashMap)
        resultModel.vertices[p.second] = p.first;

    return resultModel;
}

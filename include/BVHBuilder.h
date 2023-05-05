#pragma once

#include "ModelLoader.h"

#include <functional>
#include <fwd.hpp> //GLM
#include <vector>

#include <glm/glm.hpp>
struct AABB {
private:
    glm::vec3 min;
    glm::vec3 max;

public:
    AABB()
        : min({ 0, 0, 0 })
        , max({ 0, 0, 0 })
    {
    }

    AABB(glm::vec3 min, glm::vec3 max)
        : min(min)
        , max(max)
    {
    }

    void surrounding(AABB const& aabb)
    {
        min = glm::min(min, aabb.min);
        max = glm::max(max, aabb.max);
    }

    bool rayIntersect(glm::vec3& origin, glm::vec3& direction, float& mint)
    {
        if (glm::all(glm::greaterThan(origin, this->min)) && glm::all(glm::lessThan(origin, this->max)))
            return true;

        glm::vec3 t0 = (this->min - origin) / direction;
        glm::vec3 t1 = (this->max - origin) / direction;
        glm::vec3 tmin = glm::min(t0, t1), tmax = glm::max(t0, t1);
        float tminf = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tmaxf = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

        if (tminf > tmaxf)
            return false;

        return tminf > 0.0f;
    }

    glm::vec3& getMin() { return min; }
    glm::vec3& getMax() { return max; }
};

struct Triangle {
    glm::ivec3 triIndex;

private:
    glm::vec3 vertex1;
    glm::vec3 vertex2;
    glm::vec3 vertex3;
    int index;
    glm::vec3 center;
    AABB aabb;

public:
    Triangle(glm::vec3 vertex1, glm::vec3 vertex2, glm::vec3 vertex3,
        int index, glm::ivec3 triIndex)
        : vertex1(vertex1)
        , vertex2(vertex2)
        , vertex3(vertex3)
        , index(index)
        , triIndex(triIndex)
    {
        aabb = genAABB();
        center = genCenter();
    }

    Triangle()
        : Triangle(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0),
            glm::vec3(0, 0, 0), 0, glm::ivec3(0)) {};

    bool rayIntersect(glm::vec3& origin, glm::vec3& direction, glm::vec3& normal, float& mint);

    glm::vec3& getCenter() { return center; }
    AABB& getAABB() { return aabb; }
    int getIndex() { return index; }

    glm::vec3 getCenter() const { return center; }
    AABB getAABB() const { return aabb; }
    int getIndex() const { return index; }

private:
    glm::vec3 genCenter()
    {
        glm::vec3 sum = vertex1 + vertex2 + vertex3;
        glm::vec3 centerDim = sum / 3.0f;
        return centerDim;
    }

    AABB genAABB()
    {
        return AABB(
            glm::min(glm::min(vertex1, vertex2), vertex3),
            glm::max(glm::max(vertex1, vertex2), vertex3));
    }
};

struct Node {
    glm::vec4 triData; // for v1, v2 of left and rightTriangles
    float leftChild; // used for v0 of left triangle
    float rightChild; // used for v0 of right triangle
    AABB aabb;

    Node()
        : /*leftChildIsTriangle(false), rightChildIsTriangle(false)*/ // childIsTriangle(0)
        leftChild(0)
        , rightChild(0)
    {
        constexpr int size = sizeof(*this) / sizeof(float);
    }
};

struct Node;
class BVHBuilder {
public:
    BVHBuilder();
    void build(const Model3D& model);
    void travel(glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    void travelCycle(glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    Node* const bvhToTexture();
    int getTextureSideSize();
    const std::vector<Node>& getNodes();

private:
    void buildRecurcive(int nodeIndex, std::vector<Triangle> const& vecTriangle);
    bool travelRecurcive(Node& node, glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    bool travelStack(Node& node, glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    int texSize;
    std::vector<Node> nodeList;
    std::vector<Triangle> vecTriangle;
};

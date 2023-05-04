#pragma once
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

struct Node {
    /*bool leftChildIsTriangle;
    bool rightChildIsTriangle;*/
    float childIsTriangle;
    float leftChild;
    float rightChild;
    AABB aabb;

    Node()
        : /*leftChildIsTriangle(false), rightChildIsTriangle(false)*/ // childIsTriangle(0)
        leftChild(0)
        , rightChild(0)
    {
    }
};

struct Triangle;
struct Node;
class BVHBuilder {
public:
    BVHBuilder();
    void build(std::vector<float> const& vertexRaw);
    void travel(glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    void travelCycle(glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    Node* const bvhToTexture();
    int getNodesSize();
    const std::vector<Node> &getNodes();

private:
    void buildRecurcive(int nodeIndex, std::vector<Triangle> const& vecTriangle);
    bool travelRecurcive(Node& node, glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    bool travelStack(Node& node, glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT);
    int texSize;
    std::vector<Node> nodeList;
    std::vector<Triangle> vecTriangle;
};

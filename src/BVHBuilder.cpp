#include "BVHBuilder.h"
#include <Utils.h>
#include <algorithm>
#include <glm.hpp>
#include <stack>
using glm::vec3;

BVHBuilder::BVHBuilder() { }

void BVHBuilder::build(const Model3D& model)
{
    size_t nodeSize = sizeof(Node);
    nodeList.push_back(Node());

    const auto& v = model.vertices;
    for (int i = 0; i < model.triangles.size(); ++i) {
        const int i0 = model.triangles[i].x;
        const int i1 = model.triangles[i].y;
        const int i2 = model.triangles[i].z;
        vecTriangle.emplace_back(v[i0].position, v[i1].position, v[i2].position, i, model.triangles[i]);
    }

    nodeList.reserve(vecTriangle.size());
    buildRecurcive(0, vecTriangle);
}

void BVHBuilder::travel(glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT)
{
    travelRecurcive(nodeList[0], origin, direction, color, minT);
}

void BVHBuilder::travelCycle(glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT)
{
    travelStack(nodeList[0], origin, direction, color, minT);
}

Node* const BVHBuilder::bvhToTexture()
{
    int vertexCount = nodeList.size() * 4;
    int sqrtVertCount = ceil(sqrt(vertexCount));
    texSize = Utils::powerOfTwo(sqrtVertCount);
    nodeList.resize(texSize * texSize);

    return nodeList.data();
}

int BVHBuilder::getTextureSideSize()
{
    return texSize;
}

const std::vector<Node>& BVHBuilder::getNodes()
{
    return nodeList;
}

void BVHBuilder::buildRecurcive(int nodeIndex, std::vector<Triangle> const& vecTriangle)
{
    // Build Bpun box for triangles in vecTriangle
    AABB tempaabb = vecTriangle[0].getAABB();
    for (Triangle const& tri : vecTriangle)
        tempaabb.surrounding(tri.getAABB());

    Node& node = nodeList[nodeIndex];
    node.aabb = tempaabb;

    if (vecTriangle.size() == 2) {
        /*node.rightChildIsTriangle = true;
        node.leftChildIsTriangle = true;*/
        // node.childIsTriangle = 3;
        node.leftChild.x = -vecTriangle[0].getIndex();
        // node.triIndex//
        node.rightChild.x = -vecTriangle[1].getIndex();
        return;
    }

    // seach max dimenson for split
    vec3 maxVec = vecTriangle[0].getCenter();
    vec3 minVec = vecTriangle[0].getCenter();
    vec3 centerSum(0, 0, 0);

    for (Triangle const& tri : vecTriangle) {
        maxVec = glm::max(tri.getCenter(), maxVec);
        minVec = glm::min(tri.getCenter(), minVec);
        centerSum += tri.getCenter();
    }
    vec3 midPoint = centerSum / (float)vecTriangle.size();
    vec3 len = glm::abs(maxVec - minVec);

    int axis = 0;

    if (len.y > len.x)
        axis = 1;

    if (len.z > len.y && len.z > len.x)
        axis = 2;

    std::vector<Triangle> tempLeftTriangleList;
    std::vector<Triangle> tempRightTriangleList;

    auto splitByAxis = [&tempLeftTriangleList, &tempRightTriangleList, &midPoint, &vecTriangle](std::function<float(vec3 const& point)> getElement) {
        for (Triangle const& tri : vecTriangle) {
            if (getElement(tri.getCenter()) < getElement(midPoint))
                tempLeftTriangleList.push_back(tri);
            else
                tempRightTriangleList.push_back(tri);
        }
        assert(tempLeftTriangleList.size());
        assert(tempRightTriangleList.size());
    };

    using namespace std::placeholders;

    if (axis == 0)
        splitByAxis(bind(&vec3::x, _1));

    if (axis == 1)
        splitByAxis(bind(&vec3::y, _1));

    if (axis == 2)
        splitByAxis(bind(&vec3::z, _1));

    const auto& tl0 = tempLeftTriangleList[0];
    const auto& tr0 = tempRightTriangleList[0];
    if (tempLeftTriangleList.size() == 1) {
        node.leftChild.x = -tl0.getIndex();

    } else {
        node.leftChild.x = (int)nodeList.size();
        nodeList.emplace_back();
        buildRecurcive(nodeList.size() - 1, tempLeftTriangleList);
    }

    if (tempRightTriangleList.size() == 1) {
        node.rightChild.x = -tr0.getIndex();

    } else {
        node.rightChild.x = (int)nodeList.size();
        nodeList.emplace_back();
        buildRecurcive(nodeList.size() - 1, tempRightTriangleList);
    }
}

bool BVHBuilder::travelRecurcive(Node& node, glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT)
{
    if (!node.aabb.rayIntersect(origin, direction, minT))
        return false;

    if (node.rightChild.x <= 0)
        if (vecTriangle.at(std::abs(node.rightChild.x)).rayIntersect(origin, direction, color, minT))
            return true;

    if (node.leftChild.x <= 0)
        if (vecTriangle.at(std::abs(node.leftChild.x)).rayIntersect(origin, direction, color, minT))
            return true;

    if (node.rightChild.x > 0)
        if (travelRecurcive(nodeList[node.rightChild.x], origin, direction, color, minT))
            return true;

    if (node.leftChild.x > 0)
        if (travelRecurcive(nodeList[node.leftChild.x], origin, direction, color, minT))
            return true;

    return false;
}

bool BVHBuilder::travelStack(Node& node, glm::vec3& origin, glm::vec3& direction, glm::vec3& color, float& minT)
{
    std::stack<int> stack;
    stack.push(0);
    Node select;
    Triangle tri;
    float tempt;

    while (stack.size() != 0) {
        select = nodeList.at(stack.top());
        stack.pop();

        if (!select.aabb.rayIntersect(origin, direction, minT))
            continue;

        if (select.rightChild.x > 0 && select.leftChild.x > 0) {
            float leftMinT = 0;
            float rightMinT = 0;
            Node right = nodeList.at(select.rightChild.x);
            Node left = nodeList.at(select.leftChild.x);
            bool rightI = right.aabb.rayIntersect(origin, direction, rightMinT);
            bool leftI = left.aabb.rayIntersect(origin, direction, rightMinT);

            if (rightI)
                stack.push(select.rightChild.x);
            if (leftI)
                stack.push(select.leftChild.x);
            continue;
        }

        if (select.rightChild.x <= 0) {
            stack.push(select.rightChild.x);
        } else {
            tri = vecTriangle.at(std::abs(select.rightChild.x));
            tri.rayIntersect(origin, direction, color, minT);
        }

        if (select.leftChild.x <= 0) {
            stack.push(select.leftChild.x);
        } else {
            tri = vecTriangle.at(std::abs(select.leftChild.x));
            tri.rayIntersect(origin, direction, color, minT);
        }
    }
    return false;
}

bool Triangle::rayIntersect(vec3& origin, vec3& direction, vec3& normal, float& mint)
{
    vec3 e1 = vertex2 - vertex1;
    vec3 e2 = vertex3 - vertex1;
    vec3 P = glm::cross(direction, e2);
    float det = glm::dot(e1, P);

    if (std::abs(det) < 1e-4)
        return false;

    float inv_det = 1.0 / det;
    vec3 T = origin - vertex1;
    float u = glm::dot(T, P) * inv_det;

    if (u < 0.0 || u > 1.0)
        return false;

    vec3 Q = glm::cross(T, e1);
    float v = glm::dot(direction, Q) * inv_det;

    if (v < 0.0 || (v + u) > 1.0)
        return false;

    float tt = glm::dot(e2, Q) * inv_det;

    if (tt <= 0.0 || tt > mint)
        return false;

    normal = glm::normalize(glm::cross(e1, e2));
    mint = tt;
    return true;
}

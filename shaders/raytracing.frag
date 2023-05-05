#version 330 core

in vec2 fragCoord;
out vec4 color;

uniform vec3 location;
uniform vec2 screeResolution;
uniform mat3 viewToWorld;

uniform sampler2D texNode;
uniform ivec2 texNodeSize;


uniform sampler2D texIndices;
uniform ivec2 texIndicesSize;

uniform sampler2D texVertArray;
uniform ivec2 texVertArraySize;

//------------------- STRUCT AND LOADER BEGIN -----------------------

struct Node
{
    ivec3 leftChild;
    ivec3 rightChild;
    vec3 aabbMin;
    vec3 aabbMax;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
    float tStart;
    float tEnd;
};

struct Hit
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    bool isHit;
};


vec2 get2DIndex(int index,  ivec2 textureSize)
{
	int x = index % textureSize.x;
    int y = index / textureSize.x;
    return vec2(x / float(textureSize.x), y / float(textureSize.y));
}

Node getNode(int index)
{
	index = index * 3;
    vec4 data0 = texture(texNode, get2DIndex(index + 0, texNodeSize)).rgba;
    vec4 data1 = texture(texNode, get2DIndex(index + 1, texNodeSize)).rgba;
    vec4 data2 = texture(texNode, get2DIndex(index + 2, texNodeSize)).rgba;

	Node node;

	node.leftChild = ivec3(data0.rgb);
	node.rightChild = ivec3(data0.a, data1.rg);
	node.aabbMin = vec3(data1.ba, data2.r);
	node.aabbMax = data2.gba;
	return node;
}

struct Vertex {
    vec3 p;
    vec3 n;
    vec2 t;
};

struct IndexedTriangle
{
   Vertex v0;
   Vertex v1;
   Vertex v2;
};

IndexedTriangle getIndexedTriangle(int triIndex)
{
    ivec3 triIndices = ivec3(texture(texIndices, get2DIndex(triIndex, texIndicesSize)).rgb);
    triIndices *= 2;

    vec4 data0, data1;
    IndexedTriangle triangle;

    data0 = texture(texVertArray, get2DIndex(triIndices.r, texVertArraySize)).rgba;
    data1 = texture(texVertArray, get2DIndex(triIndices.r + 1, texVertArraySize)).rgba;
    triangle.v0.p = data0.rgb; triangle.v0.n = vec3(data0.a, data1.rg); triangle.v0.t = data1.ba;
   
    data0 = texture(texVertArray, get2DIndex(triIndices.g, texVertArraySize)).rgba;
    data1 = texture(texVertArray, get2DIndex(triIndices.g + 1, texVertArraySize)).rgba;
    triangle.v1.p = data0.rgb; triangle.v1.n = vec3(data0.a, data1.rg); triangle.v1.t = data1.ba;
   
    data0 = texture(texVertArray, get2DIndex(triIndices.b, texVertArraySize)).rgba;
    data1 = texture(texVertArray, get2DIndex(triIndices.b + 1, texVertArraySize)).rgba;
    triangle.v2.p = data0.rgb; triangle.v2.n = vec3(data0.a, data1.rg); triangle.v2.t = data1.ba;

	return triangle;
}

//------------------- STRUCT AND LOADER END -----------------------

//------------------- STACK BEGIN -----------------------
int countTI = 0;
int _stack[15];
int _index = -1;

void stackClear()
{
    _index = -1;
}

int stackSize()
{
    return _index + 1;
}

void stackPush(in int node)
{
    if(_index > 14	)
        discard;
    _stack[++_index] = node;
 
}

int stackPop()
{
    return _stack[_index--];
}
//------------------- STACK END -----------------------

bool slabs(in Ray ray, in vec3 minB, in vec3 maxB, inout float localMin) {

    if(all(greaterThan(ray.origin, minB)) && all(lessThan(ray.origin, maxB)))
        return true;

    vec3 t0 = (minB - ray.origin)/ray.direction;
    vec3 t1 = (maxB - ray.origin)/ray.direction;
    vec3 tmin = min(t0, t1), tmax = max(t0, t1);
    float tminf = max(max(tmin.x, tmin.y), tmin.z);
    float tmaxf = min(min(tmax.x, tmax.y), tmax.z);

    if (tminf > tmaxf)
        return false;

    localMin = tminf;
    return tminf < ray.tEnd && tminf > ray.tStart;
}

bool isect_tri(inout Ray ray, in IndexedTriangle tri, inout Hit hit) {
	vec3 e1 = tri.v1.p - tri.v0.p;
	vec3 e2 = tri.v2.p - tri.v0.p;
	vec3 P = cross(ray.direction, e2);
	float det = dot(e1, P);
	if (abs(det) < 1e-4)
        return false;

	float inv_det = 1. / det;
	vec3 T = (ray.origin - tri.v0.p);
	float u = dot(T, P) * inv_det;
	if (u < 0.0 || u > 1.0)
        return false;

	vec3 Q = cross(T, e1);
	float v = dot(ray.direction, Q) * inv_det;
	if (v < 0.0 || (v+u) > 1.0)
        return false;

	float tt = dot(e2, Q) * inv_det;

    if(ray.tEnd > tt && ray.tStart < tt )
    {
        vec3 c = vec3(u, v, 1.0 - u - v);
        countTI++;
        hit.position = (ray.origin + ray.direction * tt);
        hit.normal = normalize(tri.v0.n * c.z + tri.v1.n * c.x + tri.v2.n * c.y);
        hit.uv = tri.v0.t * c.z + tri.v1.t * c.x + tri.v2.t * c.y;

        hit.isHit = true;
        ray.tEnd = tt;
        return true;
    }
    return false;
}

void traceCloseHitV2(inout Ray ray, inout Hit hit)
{
    stackClear();
    stackPush(0);
    hit.isHit = false;
    Node select;
    IndexedTriangle try;
    float tempt;

    while(stackSize() != 0)
    {
        select = getNode(stackPop());
        if(!slabs(ray, select.aabbMin, select.aabbMax, tempt))
            continue;
        
        if(select.leftChild.x > 0 && select.rightChild.x > 0)
        {
            float leftMinT = 0;
            float rightMinT = 0;
            Node right = getNode(select.rightChild.x);
            Node left = getNode(select.leftChild.x);
            bool rightI = slabs(ray, right.aabbMin, right.aabbMax,  rightMinT);
            bool leftI = slabs(ray, left.aabbMin, left.aabbMax,  leftMinT);

            if(rightI && leftI)
            {
                if (rightMinT < leftMinT)
                {
                    stackPush(select.leftChild.x);
                    stackPush(select.rightChild.x);
                }
                else
                {
                    stackPush(select.rightChild.x);
                    stackPush(select.leftChild.x);
                }
                continue;
            }
            if(rightI)
                stackPush(select.rightChild.x);
            else
                stackPush(select.leftChild.x);
            continue;
        }

        if (select.rightChild.x > 0)
            stackPush(select.rightChild.x);

        if (select.leftChild.x > 0)
            stackPush(select.leftChild.x);

        if(select.rightChild.x <= 0)
        {
            try = getIndexedTriangle(abs(select.rightChild.x));
            isect_tri(ray, try, hit);
        }

        if(select.leftChild.x <= 0)
        {
            try = getIndexedTriangle(abs(select.leftChild.x));
            isect_tri(ray, try, hit);
        }
    }
}

void main() {
    vec3 viewDir = normalize(vec3((gl_FragCoord.xy - screeResolution.xy*0.5) / screeResolution.y, 1.0));
    vec3 worldDir = viewToWorld * viewDir;

    Ray ray;
    ray.direction = worldDir;
    ray.origin = location;
    ray.tStart = 0.0001;
    ray.tEnd = 10000;

    Hit hit;
    //traceCloseFor(ray, hit);
    traceCloseHitV2(ray, hit);
    //color = vec4(fragCoord,0.0,1.0);
    //color = vec4(0.5+hit.normal*0.5, 1.0);

    vec3 col = vec3(-dot(hit.normal, ray.direction));
    color = vec4(hit.normal * .5 + 0.5, 1.0);
    
    //color = vec4(hit.uv, 0., 1.0);
}

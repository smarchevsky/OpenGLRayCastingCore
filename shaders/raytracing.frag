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
    int leftChild;
    int rightChild;
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

	vec3 integerData = texture(texNode, get2DIndex(index, texNodeSize)).rgb;
	vec3 aabbMin = texture(texNode, get2DIndex(index + 1, texNodeSize)).rgb;
	vec3 aabbMax = texture(texNode, get2DIndex(index + 2, texNodeSize)).rgb;

	Node node;
	node.leftChild = int(integerData.y);
	node.rightChild = int(integerData.z);
	node.aabbMin = aabbMin;
	node.aabbMax = aabbMax;
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
    triIndices *= 3;

    IndexedTriangle triangle;
    triangle.v0.p = texture(texVertArray, get2DIndex(triIndices.r, texVertArraySize)).rgb;
    triangle.v0.n = texture(texVertArray, get2DIndex(triIndices.r + 1, texVertArraySize)).rgb;
    triangle.v0.t = texture(texVertArray, get2DIndex(triIndices.r + 2, texVertArraySize)).rg;

    triangle.v1.p = texture(texVertArray, get2DIndex(triIndices.g, texVertArraySize)).rgb;
    triangle.v1.n = texture(texVertArray, get2DIndex(triIndices.g + 1, texVertArraySize)).rgb;
    triangle.v1.t = texture(texVertArray, get2DIndex(triIndices.g + 2, texVertArraySize)).rg;

    triangle.v2.p = texture(texVertArray, get2DIndex(triIndices.b, texVertArraySize)).rgb;
    triangle.v2.n = texture(texVertArray, get2DIndex(triIndices.b + 1, texVertArraySize)).rgb;
    triangle.v2.t = texture(texVertArray, get2DIndex(triIndices.b + 2, texVertArraySize)).rg;
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
        
        if(select.leftChild > 0 && select.rightChild > 0)
        {
            float leftMinT = 0;
            float rightMinT = 0;
            Node right = getNode(select.rightChild);
            Node left = getNode(select.leftChild);
            bool rightI = slabs(ray, right.aabbMin, right.aabbMax,  rightMinT);
            bool leftI = slabs(ray, left.aabbMin, left.aabbMax,  leftMinT);

            if(rightI && leftI)
            {
                if (rightMinT < leftMinT)
                {
                    stackPush(select.leftChild);
                    stackPush(select.rightChild);
                }
                else
                {
                    stackPush(select.rightChild);
                    stackPush(select.leftChild);
                }
                continue;
            }
            if(rightI)
                stackPush(select.rightChild);
            else
                stackPush(select.leftChild);
            continue;
        }
// int leftChild; // int rightChild;
        if (select.rightChild > 0)
            stackPush(select.rightChild);

        if (select.leftChild > 0)
            stackPush(select.leftChild);

        if(select.rightChild <= 0)
        {
            try = getIndexedTriangle(abs(select.rightChild));
            isect_tri(ray, try, hit);
        }

        if(select.leftChild <= 0)
        {
            try = getIndexedTriangle(abs(select.leftChild));
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
    color = vec4(col, 1.0);
    
    color = vec4(hit.uv, 0., 1.0);
}

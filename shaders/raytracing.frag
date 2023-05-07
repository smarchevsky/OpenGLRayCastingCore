#version 330 core

in vec2 fragCoord;
out vec4 color;

uniform vec3 location;
uniform vec2 screeResolution;
uniform mat3 viewToWorld;

uniform sampler2D texGeometry;
uniform ivec2 texGeometrySize;

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

vec4 getData(int index)
{
    int x = index % texGeometrySize.x;
    int y = index / texGeometrySize.x;
    vec2 uvPos = vec2(x / float(texGeometrySize.x), y / float(texGeometrySize.y));
    return texture(texGeometry, uvPos);
}

Node getNode(int index)
{
    vec4 data0 = getData(index + 0);
    vec4 data1 = getData(index + 1);

    Node node;
    node.leftChild = floatBitsToInt(data0.r);
    node.rightChild = floatBitsToInt(data0.g);
    node.aabbMin = vec3(data0.ba, data1.r);
    node.aabbMax = data1.gba;

    return node;
}



IndexedTriangle getIndexedTriangle(int triIndex)
{
    ivec3 triIndices = floatBitsToInt(getData(triIndex).rgb);

    vec4 data0, data1;
    IndexedTriangle triangle;

    data0 = getData(triIndices.r);
    data1 = getData(triIndices.r + 1);
    triangle.v0.p = data0.rgb; triangle.v0.n = vec3(data0.a, data1.rg); triangle.v0.t = data1.ba;
   
    data0 = getData(triIndices.g);
    data1 = getData(triIndices.g + 1);
    triangle.v1.p = data0.rgb; triangle.v1.n = vec3(data0.a, data1.rg); triangle.v1.t = data1.ba;
   
    data0 = getData(triIndices.b);
    data1 = getData(triIndices.b + 1);
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
    if(_index > 14)
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
    // if(dot(cross(e1,e2), ray.direction) > 0) return false; // backface culling

	vec3 P = cross(ray.direction, e2);
	float det = dot(e1, P);
        // if (abs(det) < 1e-10) // small parts can disappear here
        //     return false;

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

    for(int i = 0; (i < 1024) && (stackSize() > 0); i++)
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

        if (select.rightChild > 0)
            stackPush(select.rightChild);

        if (select.leftChild > 0)
            stackPush(select.leftChild);

        if(select.rightChild <= 0)
        {
            try = getIndexedTriangle(-select.rightChild);
            isect_tri(ray, try, hit);
        }

        if(select.leftChild <= 0)
        {
            try = getIndexedTriangle(-select.leftChild);
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

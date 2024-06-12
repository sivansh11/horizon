struct vertex_t {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
    vec3 bi_tangent;
};

struct triangle_t {
    vertex_t p0, p1, p2;
};

struct bounding_box_t {
    vec3 min, max;
};

struct node_t {
    bounding_box_t bounding_box;
    uint primitive_count;
    uint first_index;
};

struct ray_t {
    vec3 origin, direction;
    float tmin, tmax;
};

const uint null_primitive_index = uint(-1);

struct hit_t {
    uint primitive_index;
    float t;
    float u, v, w;
};

const float infinityf = 10000000;

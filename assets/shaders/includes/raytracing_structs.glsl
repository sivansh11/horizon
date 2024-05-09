struct triangle_t {
    vec3 p0, p1, p2;
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
};

const float infinityf = 10000000;

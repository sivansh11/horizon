#version 450

#extension GL_EXT_scalar_block_layout : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

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

layout (set = 0, binding = 0, scalar) buffer triangle_buffer_t {
    triangle_t triangles[];
};

layout (set = 0, binding = 1, scalar) buffer node_buffer_t {
    node_t nodes[];
};

layout (set = 0, binding = 2, scalar) buffer primitive_index_buffer_t {
    uint primitive_indices[];
};

struct hit_t {
    uint primitive_index;
};

struct ray_t {
    vec3 origin, direction;
    float tmin, tmax;
};

bool node_intersect(in node_t node, in ray_t ray) {
    vec3 inverse_direction = vec3(1.0 / ray.direction.x, 1.0 / ray.direction.y, 1.0 / ray.direction.z);
    vec3 tmin = (node.bounding_box.min - ray.origin) * inverse_direction;
    vec3 tmax = (node.bounding_box.max - ray.origin) * inverse_direction;
    tmin = min(tmin, tmax);
    tmax = max(tmin, tmax);
    float _tmin = max(tmin.x, max(tmin.y, max(tmin.z, ray.tmin)));
    float _tmax = min(tmax.x, min(tmax.y, min(tmax.z, ray.tmax)));
    return _tmin <= _tmax;
}

bool triangle_intersect(in triangle_t triangle, inout ray_t ray) {
    vec3 e1 = triangle.p0 - triangle.p1;
    vec3 e2 = triangle.p2 - triangle.p0;
    vec3 n = cross(e1, e2);

    vec3 c = triangle.p0 - ray.origin;
    vec3 r = cross(ray.direction, c);
    float inverse_det = 1.0f / dot(n, ray.direction);

    float u = dot(r, e2) * inverse_det;
    float v = dot(r, e1) * inverse_det;
    float w = 1.0f - u - v;

    if (u >= 0 && v >= 0 && w >= 0) {
        float t = dot(n, c) * inverse_det;
        if (t >= ray.tmin && t <= ray.tmax) {
            ray.tmax = t;
            return true;
        }
    }
    return false;
}

hit_t traverse(inout ray_t ray) {
    hit_t hit;
    hit.primitive_index = uint(-1);

    uint stack[64];

    uint stack_ptr = 0;
    stack[stack_ptr++] = 0;

    while (stack_ptr != 0) {
        node_t node = nodes[--stack_ptr];
        if (!node_intersect(node, ray)) continue;

        if (node.primitive_count != 0) {
            for (uint i = 0; i < node.primitive_count; i++) {
                uint primitive_index = primitive_indices[node.first_index + i];
                if (triangle_intersect(triangles[primitive_index], ray)) 
                    hit.primitive_index = primitive_index;
            } 
        } else {
            stack[stack_ptr++] = node.first_index;
            stack[stack_ptr++] = node.first_index + 1;
        }
    }
    return hit;
}

void main() {
    vec3 eye = vec3( 0, 0, 1 );
    vec3 dir = vec3( 0, 0, -1 );
    vec3 up = vec3( 0, 1, 0 );

    vec3 right = normalize(cross(normalize(dir), normalize(up)));
    up = cross(right, normalize(dir));

    vec2 new_uv = (uv * 2) - 1;

    ray_t ray;
    ray.origin = eye;
    ray.direction = dir + (new_uv.x * right) + (new_uv.y * up);
    ray.tmin = 0;
    ray.tmax = 1000000000;

    hit_t hit = traverse(ray);
    if (hit.primitive_index != uint(-1)) 
        out_color = vec4(uv, 0, 1);
    else
        out_color = vec4(0, 0, 0, 1);
        
    // vec3 eye = vec3( 0, 0, 1 );
    // vec3 dir = vec3( 0, 0, -1 );
    // vec3 up = vec3( 0, 1, 0 );

    // ray_t ray;
    // ray.origin = eye;
    // ray.direction = dir;
    // ray.tmin = 0;
    // ray.tmax = 10000000;
    // hit_t hit = traverse(ray);
    // if (hit.primitive_index != uint(-1)) {
    //     out_color = vec4(uv, 0, 1);
    // } else {
    //     out_color = vec4(0, 0, 0, 1);
    // }
}
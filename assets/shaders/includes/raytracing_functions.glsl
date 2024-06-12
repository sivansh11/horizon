

// safe_inverse deals with divide by 0 errors
float safe_inverse(float val) {
    const float epsilon = 0.0000001;
    if (abs(val) <= epsilon) {
        if (val >= 0) return 1.0 / epsilon;
        return -1.0 / epsilon;
    } 
    return 1.0 / val;
}

// returns true if ray intersects node
bool node_intersect(in node_t node, in ray_t ray) {
    vec3 inverse_direction = vec3(safe_inverse(ray.direction.x), safe_inverse(ray.direction.y), safe_inverse(ray.direction.z));
    vec3 tmin = (node.bounding_box.min - ray.origin) * inverse_direction;
    vec3 tmax = (node.bounding_box.max - ray.origin) * inverse_direction;

    vec3 old_tmin = tmin;
    vec3 old_tmax = tmax;

    tmin = min(old_tmin, old_tmax);
    tmax = max(old_tmin, old_tmax);

    float _tmin = max(tmin.x, max(tmin.y, max(tmin.z, ray.tmin)));
    float _tmax = min(tmax.x, min(tmax.y, min(tmax.z, ray.tmax)));
    return _tmin <= _tmax;
}

struct triangle_intersect_t {
    bool intersect;
    float t;
    float u, v, w;
};

// should probably return the uvw of the triangle also
triangle_intersect_t triangle_intersect(in triangle_t triangle, inout ray_t ray) {
    triangle_intersect_t ret_intersect;

    vec3 e1 = triangle.p0.position - triangle.p1.position;
    vec3 e2 = triangle.p2.position - triangle.p0.position;
    vec3 n = cross(e1, e2);

    vec3 c = triangle.p0.position - ray.origin;
    vec3 r = cross(ray.direction, c);
    float inverse_det = 1.0f / dot(n, ray.direction);

    float u = dot(r, e2) * inverse_det;
    float v = dot(r, e1) * inverse_det;
    float w = 1.0f - u - v;

    if (u >= 0 && v >= 0 && w >= 0) {
        float t = dot(n, c) * inverse_det;
        if (t >= ray.tmin && t <= ray.tmax) {
            // ray.tmax = t;
            ret_intersect.t = t;
            ret_intersect.u = u;
            ret_intersect.v = v;
            ret_intersect.w = w;
            ret_intersect.intersect = true;
            return ret_intersect;
        }
    }
    ret_intersect.intersect = false;
    return ret_intersect;
}

vertex_t vertex_from_barry_coords(triangle_t triangle, float u, float v, float w) {
    vertex_t vertex;
    vertex.position = triangle.p0.position * u + triangle.p1.position * v + triangle.p2.position * w;
    vertex.normal = triangle.p0.normal * u + triangle.p1.normal * v + triangle.p2.normal * w;
    vertex.uv = triangle.p0.uv * u + triangle.p1.uv * v + triangle.p2.uv * w;
    vertex.tangent = triangle.p0.tangent * u + triangle.p1.tangent * v + triangle.p2.tangent * w;
    vertex.bi_tangent = triangle.p0.bi_tangent * u + triangle.p1.bi_tangent * v + triangle.p2.bi_tangent * w;
    return vertex;
}

void intesect_leaf_node(in node_t node, inout hit_t hit, inout ray_t ray) {
    for (uint i = 0; i < node.primitive_count; i++) {
        uint primitive_index = primitive_indices[node.first_index + i];
        triangle_intersect_t intersect = triangle_intersect(triangles[primitive_index], ray);
        if (intersect.intersect) {
            ray.tmax = intersect.t;
            hit.primitive_index = primitive_index;
            hit.t = intersect.t;
            hit.u = intersect.u;
            hit.v = intersect.v;
            hit.w = intersect.w;
        } 
    } 
}

hit_t stack_traverse(inout ray_t ray) {
    hit_t hit;
    hit.primitive_index = null_primitive_index;

    uint stack[32];

    uint stack_ptr = 0;
    stack[stack_ptr++] = 0;

    while (stack_ptr != 0) {
        node_t node = nodes[stack[--stack_ptr]];
        if (!node_intersect(node, ray)) continue;

        if (node.primitive_count != 0) {
            intesect_leaf_node(node, hit, ray);
        } else {
            stack[stack_ptr++] = node.first_index;
            stack[stack_ptr++] = node.first_index + 1;
        }
    }
    return hit;
}

const uint from_parent = 0;
const uint from_sibling = 1;
const uint from_child = 2;

// atm this is just randomly returning the left child, but in theory, this could be a bit faster if there was a fast way to sort both the childs and return the near child first
uint get_near_child_id(uint node_id) {
    return nodes[node_id].first_index;
}

uint get_parent_id(uint node_id) {
    return parent_ids[node_id];
}

uint get_sibling_id(uint node_id) {
    const node_t parent = nodes[get_parent_id(node_id)];
    return node_id == parent.first_index ? parent.first_index + 1 : parent.first_index;
}

hit_t stackless_traverse(inout ray_t ray) {
    hit_t hit;
    hit.primitive_index = null_primitive_index;

    const uint from_parent  = 0;
    const uint from_sibling = 1;
    const uint from_child   = 2;
    
    uint state = from_parent;
    uint current = get_near_child_id(0);

    while (true) {
        switch (state) {
            case from_child:
            {
                if (current == 0) return hit;
                if (current == get_near_child_id(get_parent_id(current))) {
                    current = get_sibling_id(current);
                    state = from_sibling;
                } else {
                    current = get_parent_id(current);
                    state = from_child;
                }
            }
            break;

            case from_sibling:
            {
                const node_t current_node = nodes[current];
                if (!node_intersect(current_node, ray)) {
                    current = get_parent_id(current);
                    state = from_child;
                } else if (current_node.primitive_count != 0) {
                    intesect_leaf_node(current_node, hit, ray);
                    current = get_parent_id(current);
                    state = from_child;
                } else {
                    current = get_near_child_id(current);
                    state = from_parent;
                }
            }
            break;

            case from_parent:
            {
                const node_t current_node = nodes[current];
                if (!node_intersect(current_node, ray)) {
                    current = get_sibling_id(current);
                    state = from_sibling;
                } else if (current_node.primitive_count != 0) {
                    intesect_leaf_node(current_node, hit, ray);
                    current = get_sibling_id(current);
                    state = from_sibling;
                } else {
                    current = get_near_child_id(current);
                    state = from_parent;
                }
            }
            break;
        }
    }
    return hit;
}

hit_t traverse(inout ray_t ray) {
    return stackless_traverse(ray);
    // return stack_traverse(ray);
}

ray_t create_ray(in vec2 uv, in mat4 inv_view) {
    vec2 px_nds = uv * 2 - 1;
    vec3 point_nds = vec3(px_nds, -1);
    vec4 point_ndsh = vec4(point_nds, 1);
    vec4 dir_eye = inv_projection * point_ndsh;
    dir_eye.w = 0;
    vec3 dir_world = vec3(inv_view * dir_eye);

    ray_t ray;
    ray.tmin = 0;
    ray.tmax = infinityf;
    ray.origin = vec3(inv_view[3][0], inv_view[3][1], inv_view[3][2]);
    ray.direction = dir_world;
    return ray;
}

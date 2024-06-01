

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

// should probably return the uvw of the triangle also
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

hit_t stack_traverse(inout ray_t ray) {
    hit_t hit;
    hit.primitive_index = null_primitive_index;

    uint stack[16];

    uint stack_ptr = 0;
    stack[stack_ptr++] = 0;

    while (stack_ptr != 0) {
        node_t node = nodes[stack[--stack_ptr]];
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

const uint from_parent = 0;
const uint from_sibling = 1;
const uint from_child = 2;

uint get_near_child_id(uint node_id) {
    return nodes[node_id].first_index;
}

uint get_parent_id(uint node_id) {
    return parent_ids[node_id];
}

uint get_sibling_id(uint node_id) {
    node_t parent = nodes[get_parent_id(node_id)];
    return node_id == parent.first_index ? parent.first_index + 1 : parent.first_index;
}

hit_t stackless_traverse(inout ray_t ray) {
    hit_t hit;
    hit.primitive_index = null_primitive_index;

    uint current = get_near_child_id(0);

    uint state = from_parent;

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
                node_t current_node = nodes[current];
                if (!node_intersect(current_node, ray)) {
                    current = get_parent_id(current);
                    state = from_child;
                } else if (current_node.primitive_count != 0) {
                    for (uint i = 0; i < current_node.primitive_count; i++) {
                        uint primitive_index = primitive_indices[current_node.first_index + i];
                        if (triangle_intersect(triangles[primitive_index], ray)) 
                            hit.primitive_index = primitive_index;
                    } 
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
                if (current == uint(-1)) return hit;
                node_t current_node = nodes[current];
                if (!node_intersect(current_node, ray)) {
                    current = get_parent_id(current);
                    state = from_sibling;
                } else if (current_node.primitive_count != 0) {
                    for (uint i = 0; i < current_node.primitive_count; i++) {
                        uint primitive_index = primitive_indices[current_node.first_index + i];
                        if (triangle_intersect(triangles[primitive_index], ray)) 
                            hit.primitive_index = primitive_index;
                    } 
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
    return stack_traverse(ray);
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

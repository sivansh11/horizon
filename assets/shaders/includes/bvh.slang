import ray;
import triangle;

struct node_intersection_t {
    bool did_intersect() {
        return tmin <= tmax;
    }
    float tmin, tmax;
}


struct node_t {
    node_intersection_t intersect(in ray_t ray) {
        float3 tmin = (_min - ray.origin) * ray.inverse_direction;
        float3 tmax = (_max - ray.origin) * ray.inverse_direction;
    
        const float3 old_tmin = tmin;
        const float3 old_tmax = tmax;
    
        tmin = min(old_tmin, old_tmax);
        tmax = max(old_tmin, old_tmax);
    
        float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
        float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));
    
        node_intersection_t node_intersection = { _tmin, _tmax };
        return node_intersection;
    }

    bool is_leaf() { return primitive_count != 0; }
    float3 _min;
    uint32_t first_index;
    float3 _max;
    uint32_t primitive_count;
};

static const uint32_t null_primitive_id = uint32_t(-1);

struct hit_t {
    bool did_intersect() { return primitive_id != null_primitive_id; }
    uint32_t primitive_id = null_primitive_id;
    float t;
    uint32_t node_intersect_count = 0;
    uint32_t primitive_intersect_count = 0;
};

void intersect_leaf_node(in const node_t *node, inout hit_t hit, bvh_t bvh, inout ray_t ray, triangle_t *triangles) {
    for (uint32_t i = 0; i < node.primitive_count; i++) {
        const uint32_t primitive_id = bvh.p_primitive_indices[node.first_index + i];
        hit.primitive_intersect_count++;
        triangle_intersection_t intersection = triangles[primitive_id].intersect(ray);
        if (intersection.did_intersect()) {
            ray.tmax = intersection.t;
            hit.primitive_id = primitive_id;
            hit.t = intersection.t;
        }
    }
}

hit_t stack_traverse_ordered(bvh_t bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    stack[stack_top++] = 0;

    while (stack_top != 0) {
        node_t *node = &bvh.p_nodes[stack[--stack_top]];

        if (node.is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, triangles);
        } else {
            hit.node_intersect_count += 2;
            node_intersection_t left = bvh.p_nodes[node.first_index].intersect(ray);
            node_intersection_t right = bvh.p_nodes[node.first_index + 1].intersect(ray);

            if (left.did_intersect()) {
                if (right.did_intersect()) {
                    if (left.tmin <= right.tmin) {
                        stack[stack_top++] = node.first_index + 1;
                        stack[stack_top++] = node.first_index;
                    } else {
                        stack[stack_top++] = node.first_index;
                        stack[stack_top++] = node.first_index + 1;
                    }
                } else {
                    stack[stack_top++] = node.first_index;
                }
            } else {
                if (right.did_intersect()) {
                    stack[stack_top++] = node.first_index + 1;
                }
            }
        }
    }
    return hit;
}

hit_t stack_traverse_ordered_optimised(bvh_t bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    uint32_t current = 0;

    while (true) {
        node_t *node = &bvh.p_nodes[current];

        if (node.is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, triangles);
            if (stack_top == 0) return hit;
            current = stack[--stack_top];
        } else {
            hit.node_intersect_count += 2;
            node_intersection_t left = bvh.p_nodes[node.first_index].intersect(ray);
            node_intersection_t right = bvh.p_nodes[node.first_index + 1].intersect(ray);

            if (left.did_intersect()) {
                if (right.did_intersect()) {
                    if (left.tmin <= right.tmin) {
                        stack[stack_top++] = node.first_index + 1;
                        current = node.first_index;
                    } else {
                        stack[stack_top++] = node.first_index;
                        current = node.first_index + 1;
                    }
                } else {
                    current = node.first_index;
                }
            } else {
                if (right.did_intersect()) {
                    current = node.first_index + 1;
                } else {
                    if (stack_top == 0) return hit;
                    current = stack[--stack_top];
                }
            }
        }
    }
    return hit;
}

hit_t stack_traverse_ordered_optimised_further(bvh_t bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;
    {
        node_t *root = &bvh.p_nodes[0];
        hit.node_intersect_count++;
        if (!root->intersect(ray).did_intersect()) return hit;
        if (root->is_leaf()) {
            intersect_leaf_node(root, hit, bvh, ray, triangles);
            return hit;
        }
    }

    uint32_t stack[32];
    uint32_t stack_top = 0;

    uint32_t current = 0;

    node_t *left_node = &bvh.p_nodes[1];
    node_t *right_node = &bvh.p_nodes[2];
    hit.node_intersect_count += 2;
    node_intersection_t left = left_node->intersect(ray);
    node_intersection_t right = right_node->intersect(ray);

    if (left.did_intersect()) {
        if (right.did_intersect()) {
            if (left.tmin <= right.tmin) {
                stack[stack_top++] = 2;
                current = 1;
            } else {
                stack[stack_top++] = 1;
                current = 2;
            }
        } else {
            current = 1;
        }
    } else {
        if (right.did_intersect()) {
            current = 2;
        } else {
            if (stack_top == 0) return hit;
            current = stack[--stack_top];
        }
    }

    while (true) {
        node_t *node = &bvh.p_nodes[current];

        hit.node_intersect_count += 2;
        node_t *left_node = &bvh.p_nodes[node.first_index];
        node_t *right_node = &bvh.p_nodes[node.first_index + 1];
        node_intersection_t left = left_node->intersect(ray);
        node_intersection_t right = right_node->intersect(ray);

        uint32_t start = left_node.first_index;
        uint32_t end = start + int(left.did_intersect()) * left_node.primitive_count + int(right.did_intersect()) * right_node.primitive_count;

        for (uint32_t i = start; i < end; i++) {
            const uint32_t primitive_id = bvh.p_primitive_indices[i];
            hit.primitive_intersect_count++;
            triangle_intersection_t intersection = triangles[primitive_id].intersect(ray);
            if (intersection.did_intersect()) {
                ray.tmax = intersection.t;
                hit.primitive_id = primitive_id;
                hit.t = intersection.t;
            }
        }

        bool should_traverse_left = left.did_intersect() && !left_node.is_leaf() ? true : false;
        bool should_traverse_right = right.did_intersect() && !right_node.is_leaf() ? true : false;

        if (should_traverse_left) {
            if (should_traverse_right) {
                if (left.tmin <= right.tmin) {
                    stack[stack_top++] = node.first_index + 1;
                    current = node.first_index;
                } else {
                    stack[stack_top++] = node.first_index;
                    current = node.first_index + 1;
                }
            } else {
                current = node.first_index;
            }
        } else {
            if (should_traverse_right) {
                current = node.first_index + 1;
            } else {
                if (stack_top == 0) return hit;
                current = stack[--stack_top];
            }
        }
    }
    return hit;
}

hit_t stack_traverse_unordered(bvh_t bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    stack[stack_top++] = 0;

    while (stack_top != 0) {
        node_t *node = &bvh.p_nodes[stack[--stack_top]];

        hit.node_intersect_count++;
        if (!node->intersect(ray).did_intersect()) continue;

        if (node->is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, triangles);
        } else {
            stack[stack_top++] = node->first_index;
            stack[stack_top++] = node->first_index + 1;
        }
    }
    return hit;
}

uint32_t get_near_child_id(uint32_t node_id, node_t *p_nodes) {
    return p_nodes[node_id].first_index;
}

uint32_t get_parent_id(uint32_t node_id, uint32_t *p_parent_ids) {
    return p_parent_ids[node_id];
}

uint32_t get_sibling_id(uint32_t node_id, node_t *p_nodes, uint32_t *p_parent_ids) {
    node_t *parent = &p_nodes[p_parent_ids[node_id]];
    uint32_t left = parent->first_index;
    uint32_t right = parent->first_index + 1;
    return node_id == left ? right : left;
}

hit_t stackless_traverse(bvh_t bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    const uint from_parent = 0;
    const uint from_sibling = 1;
    const uint from_child = 2;

    uint state = from_parent;
    uint32_t current_id = get_near_child_id(0, bvh.p_nodes);

    while (true) {
        if (state == from_child)
        {
            if (current_id == 0) return hit;
            if (current_id == get_near_child_id(get_parent_id(current_id, bvh.p_parent_ids), bvh.p_nodes)) {
                current_id = get_sibling_id(current_id, bvh.p_nodes, bvh.p_parent_ids);
                state = from_sibling;
            } else {
                current_id = get_parent_id(current_id, bvh.p_parent_ids);
                state = from_child;
            }
        }

        if (state == from_sibling)
        {
            node_t current_node = bvh.p_nodes[current_id];
            hit.node_intersect_count++;
            if (!current_node.intersect(ray).did_intersect()) {
                current_id = get_parent_id(current_id, bvh.p_parent_ids);
                state = from_child;
            } else if (current_node.is_leaf()) {
                for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                    const uint32_t primitive_id = bvh.p_primitive_indices[current_node.first_index + i];
                    hit.primitive_intersect_count++;
                    triangle_intersection_t intersection = triangles[primitive_id].intersect(ray);
                    if (intersection.did_intersect()) {
                        ray.tmax = intersection.t;
                        hit.primitive_id = primitive_id;
                    }
                }
                current_id = get_parent_id(current_id, bvh.p_parent_ids);
                state = from_child;
            } else {
                current_id = get_near_child_id(current_id, bvh.p_nodes);
                state = from_parent;
            }
        }

        if (state == from_parent)
        {
            node_t current_node = bvh.p_nodes[current_id];
            hit.node_intersect_count++;
            if (!current_node.intersect(ray).did_intersect()) {
                current_id = get_sibling_id(current_id, bvh.p_nodes, bvh.p_parent_ids);
                state = from_sibling;
            } else if (current_node.is_leaf()) {
                for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                    const uint32_t primitive_id = bvh.p_primitive_indices[current_node.first_index + i];
                    hit.primitive_intersect_count++;
                    triangle_intersection_t intersection = triangles[primitive_id].intersect(ray);
                    if (intersection.did_intersect()) {
                        ray.tmax = intersection.t;
                        hit.primitive_id = primitive_id;
                    }
                }
                current_id = get_sibling_id(current_id, bvh.p_nodes, bvh.p_parent_ids);
                state = from_sibling;
            } else {
                current_id = get_near_child_id(current_id, bvh.p_nodes);
                state = from_parent;
            }
        }
    }
    return hit;
}

struct bvh_t {
    hit_t traverse(inout ray_t ray, triangle_t *triangles) {
        return stack_traverse_ordered_optimised(this, ray, triangles);
    }

    node_t *p_nodes;
    uint32_t *p_primitive_indices;
    uint32_t node_count;
    uint32_t primitive_count;
    uint32_t *p_parent_ids;
};


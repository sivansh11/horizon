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

layout (set = 0, binding = 0, scalar) readonly buffer triangle_buffer_t {
    triangle_t triangles[];
};

layout (set = 0, binding = 1, scalar) readonly buffer node_buffer_t {
    node_t nodes[];
};

layout (set = 0, binding = 2, scalar) readonly buffer primitive_index_buffer_t {
    uint primitive_indices[];
};

layout (set = 1, binding = 0, scalar) uniform camera_buffer_t {
    mat4 projection;
    mat4 inv_projection;
    mat4 view;
    mat4 inv_view;
};

// layout (set = 2, binding = 0, scalar) uniform extra_t {
//     vec3 position;
// };

struct hit_t {
    uint primitive_index;
};

struct ray_t {
    vec3 origin, direction;
    float tmin, tmax;
};

float safe_inverse(float x) {
    // Use abs instead of std::fabs
    const float epsilon = 0.001; // Adjust epsilon as needed
    if (abs(x) <= epsilon) {
        float sign_x = 0;
        if (x >= 0) sign_x = 1; 
        if (x < 0) sign_x = -1; 
        float ret_x = sign_x / epsilon;
        return ret_x; 
    } else {
        return 1.0 / x;
    }
    return abs(x) <= epsilon ? sign(x) / epsilon : 1.0 / x;
}

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

    node_t node0 = nodes[0];
    node_t node1 = nodes[1];
    node_t node2 = nodes[2];

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

ray_t create_ray() {
    vec2 px_nds = uv * 2 - 1;
    vec3 point_nds = vec3(px_nds, -1);
    vec4 point_ndsh = vec4(point_nds, 1);
    vec4 dir_eye = inv_projection * point_ndsh;
    dir_eye.w = 0;
    vec3 dir_world = vec3(inv_view * dir_eye);

    ray_t ray;
    ray.tmin = 0;
    ray.tmax = 10000000;
    ray.origin = vec3(inv_view[3][0], inv_view[3][1], inv_view[3][2]);
    ray.direction = dir_world;
    return ray;
}

vec3 createRay(vec2 px, mat4 PInv, mat4 VInv)
{
  
    // convert pixel to NDS
    // [0,1] -> [-1,1]
    vec2 pxNDS = px*2. - 1.;

    // choose an arbitrary point in the viewing volume
    // z = -1 equals a point on the near plane, i.e. the screen
    vec3 pointNDS = vec3(pxNDS, -1.);

    // as this is in homogenous space, add the last homogenous coordinate
    vec4 pointNDSH = vec4(pointNDS, 1.0);
    // transform by inverse projection to get the point in view space
    vec4 dirEye = PInv * pointNDSH;

    // since the camera is at the origin in view space by definition,
    // the current point is already the correct direction 
    // (dir(0,P) = P - 0 = P as a direction, an infinite point,
    // the homogenous component becomes 0 the scaling done by the 
    // w-division is not of interest, as the direction in xyz will 
    // stay the same and we can just normalize it later
    dirEye.w = 0.;

    // compute world ray direction by multiplying the inverse view matrix
    vec3 dirWorld = (VInv * dirEye).xyz;

    // now normalize direction
    return normalize(dirWorld); 
}

mat4 perspective(float fovy, float aspect, float zNear, float zFar)
{
    float tanHalfFovy = tan(fovy / 2.f);

    mat4 Result = mat4(0.f);
    Result[0][0] = 1.f / (aspect * tanHalfFovy);
    Result[1][1] = 1.f / (tanHalfFovy);
    Result[2][2] = - (zFar + zNear) / (zFar - zNear);
    Result[2][3] = - 1.f;
    Result[3][2] = - (2.f * zFar * zNear) / (zFar - zNear);
    return Result;
}

mat4 lookAt(vec3 eye, vec3 center, vec3 up)
{
    vec3 f = vec3(normalize(center - eye));
    vec3 s = vec3(normalize(cross(f, up)));
    vec3 u = vec3(cross(s, f));

    mat4 Result = mat4(1);
    Result[0][0] = s.x;
    Result[1][0] = s.y;
    Result[2][0] = s.z;
    Result[0][1] = u.x;
    Result[1][1] = u.y;
    Result[2][1] = u.z;
    Result[0][2] =-f.x;
    Result[1][2] =-f.y;
    Result[2][2] =-f.z;
    Result[3][0] =-dot(s, eye);
    Result[3][1] =-dot(u, eye);
    Result[3][2] = dot(f, eye);
    return Result;
}

float col(uint c) {
    c = c % 255;
    return float(c) / 255.f;
}

void main() {
    ray_t ray = create_ray();

    hit_t hit = traverse(ray);
    if (hit.primitive_index != uint(-1)) 
        out_color = vec4(col((hit.primitive_index + 1) * 37), col((hit.primitive_index + 1) * 91), col((hit.primitive_index + 1) * 51), 1);
    else
        out_color = vec4(0, 0, 0, 1);

    
    // vec3 position = vec3(0, 0, -5);
    // vec3 front = vec3(0, 0, 1);
    // mat4 view = lookAt(position, position + front, vec3(0, 1, 0));
    // mat4 projection = perspective(radians(45.f), 1.52380955f, 0.100000001f, 1000.f);


    // ray_t ray;
    // ray.tmin = 0;
    // ray.tmax = 10000000;
    // ray.direction = createRay(vec2(0.5f, 0.5f), inverse(projection), inverse(view));
    // ray.origin = position;

    // hit_t hit = traverse(ray);
    // if (hit.primitive_index != uint(-1)) {
    //     out_color = vec4(1, 1, 1, 1);
    // } else {
    //     out_color = vec4(0, 0, 0, 1);
    // }
    

    // vec3 position = vec3(0, 0, -5);
    // vec3 front = vec3(0, 0, 1);
    // mat4 view = lookAt(position, position + front, vec3(0, 1, 0));
    // mat4 projection = perspective(radians(45.f), 1.52380955f, 0.100000001f, 1000.f);


    // ray_t ray;
    // ray.tmin = 0;
    // ray.tmax = 10000000;
    // ray.direction = createRay(vec2(0.5f, 0.5f), inverse(projection), inverse(view));
    // ray.origin = position;

    // // if (triangles[0].intersect(ray)) {
    // if (node_intersect(nodes[0], ray)) {
    //     out_color = vec4(1, 1, 1, 1);
    // } else {
    //     out_color = vec4(0, 0, 0, 1);
    // }

    
    // vec3 position = vec3(0, 0, -5);
    // vec3 front = vec3(0, 0, 1);
    // mat4 view = lookAt(position, position + front, vec3(0, 1, 0));
    // mat4 projection = perspective(radians(45.f), 1.52380955f, 0.100000001f, 1000.f);


    // ray_t ray;
    // ray.tmin = 0;
    // ray.tmax = 10000000;
    // ray.direction = createRay(vec2(0.5f, 0.5f), inverse(projection), inverse(view));
    // ray.origin = position;

    // // if (triangles[0].intersect(ray)) {
    // if (triangle_intersect(triangles[0], ray)) {
    //     out_color = vec4(1, 1, 1, 1);
    // } else {
    //     out_color = vec4(0, 0, 0, 1);
    // }


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
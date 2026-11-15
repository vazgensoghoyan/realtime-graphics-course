struct VertexIn {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) texcoord: vec2f,
    @location(3) tangent: vec3f,
}

struct VertexOut {
    @builtin(position) clip_position: vec4f,
    @location(0) world_position: vec3f,
    @location(1) normal: vec3f,
    @location(2) texcoord: vec2f,
    @location(3) tangent: vec3f,
}

struct Camera {
    viewProjection: mat4x4f,
    position: vec3f,
}

@group(0) @binding(0) var<uniform> camera: Camera;
@group(0) @binding(1) var linearSampler: sampler;
@group(0) @binding(2) var albedoTexture: texture_2d<f32>;
@group(0) @binding(3) var normalTexture: texture_2d<f32>;
@group(0) @binding(4) var metalnessTexture: texture_2d<f32>;
@group(0) @binding(5) var environmentTexture: texture_2d<f32>;

const PI = 3.14159265358979323846;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    return VertexOut(
        camera.viewProjection * vec4f(in.position, 1.0),
        in.position,
        in.normal,
        in.texcoord,
        in.tangent
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    // normal mapping

    let normal = normalize(in.normal);

    var normal_map_val = textureSample(normalTexture, linearSampler, in.texcoord).xyz;
    normal_map_val = 2 * normal_map_val - vec3(1.0);

    let bitangent = cross(normal, in.tangent);
    let tbn = mat3x3f(in.tangent, bitangent, normal);

    let real_normal = tbn * normal_map_val;

    // color

    let albedo = textureSample(albedoTexture, linearSampler, in.texcoord).rgb;

    let light_direction = normalize(vec3f(0.9, 0.4, 0.0));
    let albedo_color = (0.2 + 0.8 * max(0.0, dot(real_normal, light_direction))) * albedo;

    let metalness = textureSample(metalnessTexture, linearSampler, in.texcoord).x;

    let camera_to_pixel_dir = normalize(in.world_position - camera.position);
    let refl_dir = reflect(camera_to_pixel_dir, real_normal);

    let u = atan2(refl_dir.z, refl_dir.x) / PI * 0.5 + 0.5;
    let v = -atan2(refl_dir.y, length(refl_dir.xz)) / PI + 0.5;

    let reflected_color = textureSample(environmentTexture, linearSampler, vec2f(u, v)).rgb;
    let final_color = mix(albedo_color, reflected_color, metalness);

    return vec4f(final_color, 1.0);
}

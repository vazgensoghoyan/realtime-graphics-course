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
    //let normal = normalize(in.normal);

    //let albedo = textureSample(albedoTexture, linearSampler, in.texcoord).rgb;

    //let light_direction = normalize(vec3f(0.9, 0.4, 0.0));
    //let color = (0.2 + 0.8 * max(0.0, dot(normal, light_direction))) * albedo;

    var normal_map_val = textureSample(normalTexture, linearSampler, in.texcoord).xyz;
    normal_map_val = 2 * normal_map_val - vec3(1.0);

    let bitangent = cross(in.normal, in.tangent);
    let tbn = mat3x3f(in.tangent, bitangent, in.normal);

    let real_normal = tbn * normal_map_val;

    let color = real_normal * 0.5 + vec3(0.5);
    return vec4f(color, 1.0);
}

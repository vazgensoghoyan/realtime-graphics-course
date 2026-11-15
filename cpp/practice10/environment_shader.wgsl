struct VertexOut {
    @builtin(position) clip_position: vec4f,
    @location(0) ndc: vec2f,
}

struct Camera {
    viewProjection: mat4x4f,
    inverseViewProjection: mat4x4f,
    position: vec3f,
}

@group(0) @binding(0) var<uniform> camera: Camera;
@group(0) @binding(1) var linearSampler: sampler;
@group(0) @binding(5) var environmentTexture: texture_2d<f32>;

const PI = 3.14159265358979323846;

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32) -> VertexOut {
    var positions = array<vec2f, 3>(
        vec2f(-1.0, -1.0),
        vec2f(3.0, -1.0),
        vec2f(-1.0, 3.0),
    );

    let ndc = positions[vertex_index];
    return VertexOut(vec4f(ndc, 0.0, 1.0), ndc);
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let clip_position = vec4f(in.ndc, 1.0, 1.0);
    let world_position = camera.inverseViewProjection * clip_position;

    let dir = normalize(world_position.xyz / world_position.w - camera.position);

    let u = atan2(dir.z, dir.x) / PI * 0.5 + 0.5;
    let v = -atan2(dir.y, length(dir.xz)) / PI + 0.5;

    let env_color = textureSample(environmentTexture, linearSampler, vec2f(u, v)).rgb;

    return vec4f(env_color, 1.0);
}

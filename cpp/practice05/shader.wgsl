struct VertexIn {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) texcoord: vec2f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) texcoord: vec2f,
}

struct UniformBuffer {
    model: mat4x4f,
    viewProjection: mat4x4f,
}

@group(0) @binding(0) var<uniform> uniformBuffer: UniformBuffer;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    return VertexOut(
        uniformBuffer.viewProjection * uniformBuffer.model * vec4f(in.position, 1.0),
        normalize((uniformBuffer.model * vec4f(in.normal, 0.0)).xyz),
        in.texcoord
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    // let normal = normalize(in.normal);

    // let albedo = vec3f(1.0);

    // let light_dir = normalize(vec3f(3.0, 2.0, 1.0));
    // let color = (0.5 + 0.5 * dot(normal, light_dir)) * albedo;

    return vec4f(in.texcoord, 0.0, 1.0);
}

struct Immediates {
    shadowProjection: mat4x4f,
}

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(@location(0) position: vec3f) -> @builtin(position) vec4f {
    return immediates.shadowProjection * vec4f(position, 1.0);
}

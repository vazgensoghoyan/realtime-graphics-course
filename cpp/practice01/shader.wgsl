@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32)
    -> @builtin(position) vec4f
{
    var positions = array<vec2f, 3>(
        vec2f(0.0, 0.5),
        vec2f(-0.5, -0.5),
        vec2f(0.5, -0.5),
    );
    return vec4f(positions[vertexIndex], 0.0, 1.0);
}

@fragment
fn fragmentMain() -> @location(0) vec4f {
    return vec4f(0.95, 0.35, 0.15, 1.0);
}

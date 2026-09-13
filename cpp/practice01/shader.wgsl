@vertex
fn vertexMain(
    @builtin(vertex_index) vertexIndex: u32
) -> @builtin(position) vec4f {
    var positions = array<vec2f, 3>(
        vec2f(0.0, 0.5),
        vec2f(-0.5, -0.5),
        vec2f(0.5, -0.5),
    );
    return vec4f(positions[vertexIndex], 0.0, 1.0);
}

@fragment
fn fragmentMain(
    @builtin(position) position: vec4f
) -> @location(0) vec4f {
    let cellSize = 50.0;

    let x = i32(modf(position.x / cellSize).whole);
    let y = i32(modf(position.y / cellSize).whole);

    if ((x % 2) == (y % 2)) {
        return vec4f(1, 1, 1, 1.0);
    }
    return vec4f(0, 0, 0, 1.0);
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
}

const POSITIONS = array<vec2f, 7>(
    vec2f(0.0, 1.0),
    vec2f(-sqrt(0.75), 0.5),
    vec2f(-sqrt(0.75), -0.5),
    vec2f(0.0, -1.0),
    vec2f( sqrt(0.75), -0.5),
    vec2f( sqrt(0.75), 0.5),
    vec2f(0.0, 0.0),
);

const INDICES = array<u32, 18>(
    6, 0, 1,
    6, 1, 2,
    6, 2, 3,
    6, 3, 4,
    6, 4, 5,
    6, 5, 0,
);

const COLORS = array<vec4f, 7>(
    vec4f(1.0, 0.0, 0.0, 1.0), // red
    vec4f(1.0, 1.0, 0.0, 1.0), // yellow
    vec4f(0.0, 1.0, 0.0, 1.0), // green
    vec4f(0.0, 1.0, 1.0, 1.0), // этот голубой
    vec4f(0.0, 0.0, 1.0, 1.0), // blue
    vec4f(1.0, 0.0, 1.0, 1.0), // розовый
    vec4f(1.0, 1.0, 1.0, 1.0), // white (center)
);

struct Immediate {
    transform: mat4x4<f32>,
    view: mat4x4<f32>,
}
var<immediate> IMM: Immediate;

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOut {
    let index = INDICES[vertexIndex];
    return VertexOut(
        vec4f(POSITIONS[index], 0.0, 1.0) * IMM.transform * IMM.view,
        COLORS[index]
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return in.color;
}

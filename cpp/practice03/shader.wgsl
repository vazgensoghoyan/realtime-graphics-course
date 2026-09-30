struct VertexIn {
    @location(0) position: vec2f,
    @location(1) color: vec4f,
    @location(2) distance: f32,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
    @location(1) @interpolate(linear) distance: f32,
}

struct Immediates {
    view: mat4x4f,
    time: f32,
    dashed: u32,
}
var<immediate> immediates: Immediates;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    return VertexOut(
        immediates.view * vec4f(in.position, 0.0, 1.0),
        in.color,
        in.distance
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    if (immediates.dashed != 0u) {
        // штрих по 20px линия и 20px пробел, скорость 40 px/сек
        let length = 20.0;
        let velocity = 40.0;

        let phase = modf((in.distance + immediates.time * velocity) / (2 * length)).fract;
        if (phase >= 0.5) { // делим пополам отрезок длиной 2*length
            discard;
        }
    }

    return in.color;
}

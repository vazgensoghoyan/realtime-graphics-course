struct VertexIn {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) texcoord: vec2f,
}

struct VertexOut {
    @builtin(position) clip_position: vec4f,
    @location(0) world_position: vec3f,
    @location(1) normal: vec3f,
}

struct CameraUniforms {
    viewProjection: mat4x4f,
    position: vec3f,
}

struct PointLight {
    position: vec3f,
    intensity: f32,
    attenuation: vec3f,
}

struct LightsUniforms {
    lightDirection: vec3f,
    ambientIntensity: vec3f,
    intensity: f32,
    pointLight: PointLight,
}

struct Immediates {
    model: mat4x4f,
    albedo: vec3f,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var<uniform> lights: LightsUniforms;

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let world_position = immediates.model * vec4f(in.position, 1.0);
    return VertexOut(
        camera.viewProjection * world_position,
        world_position.xyz,
        normalize((immediates.model * vec4f(in.normal, 0.0)).xyz)
    );
}

fn computePointLighting(vertex: vec3f, normal: vec3f, light: PointLight) -> f32 {
    let to_light = light.position - vertex;
    let dir = normalize(to_light);
    let d = length(to_light);

    let c0 = light.attenuation.x;
    let c1 = light.attenuation.y;
    let c2 = light.attenuation.z;

    let attenuation = light.intensity / (c0 + c1 * d + c2 * d * d);

    return attenuation * max(0.0, dot(normal, dir));
}

fn computeDirectionalLighting(normal: vec3f, light_dir: vec3f, intensity: f32) -> vec3f {
    return intensity * vec3f(max(0.0, dot(normal, light_dir)));
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let normal = normalize(in.normal);

    let pointLighting = computePointLighting(in.world_position, normal, lights.pointLight);
    let sun = computeDirectionalLighting(normal, lights.lightDirection, lights.intensity);

    let color = immediates.albedo * (lights.ambientIntensity + sun + vec3f(pointLighting));

    return vec4f(color, 1.0);
}

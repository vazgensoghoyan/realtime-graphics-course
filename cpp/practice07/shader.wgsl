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
    intensity: vec3f,
    attenuation: vec3f,
}

struct LightsUniforms {
    lightDirection: vec3f,
    ambientIntensity: vec3f,
    intensity: vec3f,
    pointLight: PointLight,
}

struct Immediates {
    model: mat4x4f,
    albedo: vec3f,
    roughness: f32,
    specularFactor: f32,
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

const PI = 3.1415926535;

fn computeLighting(normal: vec3f, lightDirection: vec3f, lightIntensity: vec3f, viewDirection: vec3f) -> vec3f {
    let cos = max(0.0, dot(normal, lightDirection));
    let halfway = normalize(lightDirection + viewDirection);
    let power = 1.0 / max(0.0001, immediates.roughness * immediates.roughness) - 1.0;
    let specular = immediates.specularFactor
        * (power + 2.0) / (2.0 * PI)
        * pow(max(0.0, dot(normal, halfway)), power);

    return lightIntensity * cos * (immediates.albedo + vec3f(specular));
}

fn computePointLighting(vertex: vec3f, normal: vec3f, viewDirection: vec3f, light: PointLight) -> vec3f {
    let to_light = light.position - vertex;
    let dir = normalize(to_light);
    let d = length(to_light);

    let c0 = light.attenuation.x;
    let c1 = light.attenuation.y;
    let c2 = light.attenuation.z;

    let attenuatedIntensity = light.intensity / (c0 + c1 * d + c2 * d * d);

    return computeLighting(normal, dir, attenuatedIntensity, viewDirection);
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let normal = normalize(in.normal);
    let viewDirection = normalize(camera.position - in.world_position);

    let pointLighting = computePointLighting(in.world_position, normal, viewDirection, lights.pointLight);
    let sun = computeLighting(normal, lights.lightDirection, lights.intensity, viewDirection);

    let color = immediates.albedo * lights.ambientIntensity + sun + pointLighting;

    return vec4f(color, 1.0);
}

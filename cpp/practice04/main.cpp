#include <wgpu_app.hpp>
#include <file_utils.hpp>
#include <obj_loader.hpp>

#include <webgpu.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <unordered_set>

static std::filesystem::path const projectRoot = PROJECT_ROOT;

WGPUShaderModule createShaderModule(WGPUDevice device, std::filesystem::path const &path) {
    auto const source = loadFile(path);

    WGPUShaderSourceWGSL shaderSourceWGSL = WGPU_SHADER_SOURCE_WGSL_INIT;
    shaderSourceWGSL.code = {source.data(), source.size()};

    WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderModuleDescriptor.nextInChain = &shaderSourceWGSL.chain;

    return wgpuDeviceCreateShaderModule(device, &shaderModuleDescriptor);
}

WGPURenderPipeline createPipeline(WGPUDevice device, WGPUShaderModule shaderModule,
                                  WGPUTextureFormat surfaceFormat) {
    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = 128;

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPUVertexAttribute attributes[2] = {
        WGPU_VERTEX_ATTRIBUTE_INIT,
        WGPU_VERTEX_ATTRIBUTE_INIT,
    };

    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[0].offset = offsetof(ObjVertex, position);
    attributes[0].shaderLocation = 0;

    attributes[1].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = offsetof(ObjVertex, normal);
    attributes[1].shaderLocation = 1;

    WGPUVertexBufferLayout vertexLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexLayout.arrayStride = sizeof(ObjVertex);
    vertexLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount = 2;
    vertexLayout.attributes = attributes;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.vertex.bufferCount = 1;
    renderPipelineDescriptor.vertex.buffers = &vertexLayout;
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.fragment = &fragmentState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

struct WgpuBufferWrapper {
    WGPUBuffer buffer;
    std::size_t bytes;
};

WgpuBufferWrapper initMeshVertexBuffer(const WgpuApp& app, const ObjMesh& mesh) {
    auto const bytes = mesh.vertices.size() * sizeof(ObjVertex);

    WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    desc.size = bytes;
    desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(app.device(), &desc);
    wgpuQueueWriteBuffer(app.queue(), buffer, 0, mesh.vertices.data(), bytes);

    return {buffer, bytes};
}

WgpuBufferWrapper initMeshIndexBuffer(const WgpuApp& app, const ObjMesh& mesh) {
    auto const bytes = mesh.indices.size() * sizeof(std::uint32_t);

    WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    desc.size = bytes;
    desc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(app.device(), &desc);
    wgpuQueueWriteBuffer(app.queue(), buffer, 0, mesh.indices.data(), bytes);

    return {buffer, bytes};
}

int main() try {
    WgpuApp app("Practice03", 1280, 720, true);

    WGPUShaderModule shaderModule = createShaderModule(app.device(), projectRoot / "shader.wgsl");
    WGPURenderPipeline renderPipeline = createPipeline(app.device(), shaderModule, app.surfaceFormat());

    ObjMesh bunny = loadObj(projectRoot / "bunny.obj");

    WgpuBufferWrapper bunnyVertexBuffer = initMeshVertexBuffer(app, bunny);
    WgpuBufferWrapper bunnyIndexBuffer = initMeshIndexBuffer(app, bunny);

    auto lastFrameStart = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    std::unordered_set<SDL_Keycode> keydown;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                app.resize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                keydown.insert(event.key.key);
                break;
            case SDL_EVENT_KEY_UP:
                keydown.erase(event.key.key);
                break;
            }
        }

        std::optional<WGPUSurfaceTexture> surfaceTexture = app.beginFrame();
        if (!surfaceTexture) {
            continue;
        }

        auto const now = std::chrono::high_resolution_clock::now();
        float const dt = std::chrono::duration<float>(now - lastFrameStart).count();
        time += dt;
        lastFrameStart = now;

        math::matrix4f const model{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        math::matrix4f const view{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        math::matrix4f const projection{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.01, 0.02, 0.03, 1.0};

        WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);

        auto modelTranspose = math::transpose(model);
        auto viewProjectionTranspose = math::transpose(projection * view);
        wgpuRenderPassEncoderSetImmediates(renderPass, 0, &modelTranspose, sizeof(modelTranspose));
        wgpuRenderPassEncoderSetImmediates(renderPass, sizeof(modelTranspose), &viewProjectionTranspose, sizeof(viewProjectionTranspose));

        wgpuRenderPassEncoderSetVertexBuffer(
            renderPass, 0, bunnyVertexBuffer.buffer, 0, bunnyVertexBuffer.bytes
        );

        wgpuRenderPassEncoderSetIndexBuffer(
            renderPass, bunnyIndexBuffer.buffer, WGPUIndexFormat_Uint32, 0, bunnyIndexBuffer.bytes
        );

        wgpuRenderPassEncoderDrawIndexed(
            renderPass,
            static_cast<std::uint32_t>(bunny.indices.size()),
            1, 0, 0, 0
        );

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(app.queue(), 1, &commandBuffer);
        wgpuCommandBufferRelease(commandBuffer);

        wgpuSurfacePresent(app.surface());

        wgpuTextureViewRelease(targetView);
        wgpuTextureRelease(surfaceTexture->texture);
    }

    wgpuBufferRelease(bunnyIndexBuffer.buffer);
    wgpuBufferRelease(bunnyVertexBuffer.buffer);
    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);
} catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

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

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.vertex.bufferCount = 1;
    renderPipelineDescriptor.vertex.buffers = &vertexLayout;
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    /*can be removed for normal view of bunny*/
    /*from here*/
    // renderPipelineDescriptor.primitive.cullMode = WGPUCullMode_Front; // wanted to remove
    /*to here*/
    renderPipelineDescriptor.fragment = &fragmentState;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

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

WGPUTexture createDepthTexture(const WgpuApp& app) {
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size.width = app.width();
    desc.size.height = app.height();
    desc.size.depthOrArrayLayers = 1;
    desc.format = WGPUTextureFormat_Depth24Plus;
    desc.usage = WGPUTextureUsage_RenderAttachment;

    return wgpuDeviceCreateTexture(app.device(), &desc);
}

WGPUTextureView createDepthView(WGPUTexture depthBuffer) {
    WGPUTextureViewDescriptor desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    desc.usage = WGPUTextureUsage_RenderAttachment;
    desc.dimension = WGPUTextureViewDimension_2D;
    desc.format = WGPUTextureFormat_Depth24Plus;
    desc.aspect = WGPUTextureAspect_DepthOnly;
    desc.mipLevelCount = 1;
    desc.arrayLayerCount = 1;

    return wgpuTextureCreateView(depthBuffer, &desc);
}

int main() try {
    WgpuApp app("Practice03", 1280, 720, true);

    WGPUShaderModule shaderModule = createShaderModule(app.device(), projectRoot / "shader.wgsl");
    WGPURenderPipeline renderPipeline = createPipeline(app.device(), shaderModule, app.surfaceFormat());

    ObjMesh bunny = loadObj(projectRoot / "bunny.obj");
    float bunny_x[3] = { -1.5f, 0.f, 1.5f };
    float bunny_y[3] = { 0.f, 0.f, 0.f };
    int selectedBunny = 0; // by pressing '1', '2', '3' we can select which bunny to move

    WgpuBufferWrapper bunnyVertexBuffer = initMeshVertexBuffer(app, bunny);
    WgpuBufferWrapper bunnyIndexBuffer = initMeshIndexBuffer(app, bunny);

    WGPUTexture depthBuffer = createDepthTexture(app);
    WGPUTextureView depthBufferView = createDepthView(depthBuffer);

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
                if (!event.key.repeat) {
                    if (event.key.key == SDLK_1) selectedBunny = 0;
                    if (event.key.key == SDLK_2) selectedBunny = 1;
                    if (event.key.key == SDLK_3) selectedBunny = 2;
                }
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

        if (wgpuTextureGetWidth(depthBuffer) != app.width() || wgpuTextureGetHeight(depthBuffer) != app.height()) {
            wgpuTextureViewRelease(depthBufferView);
            wgpuTextureRelease(depthBuffer);
            depthBuffer = createDepthTexture(app);
            depthBufferView = createDepthView(depthBuffer);
        }

        auto const now = std::chrono::high_resolution_clock::now();
        float const dt = std::chrono::duration<float>(now - lastFrameStart).count();
        time += dt;
        lastFrameStart = now;

        float speed = 1.5f;
        if (keydown.contains(SDLK_SPACE)) speed = 3.0f;

        if (keydown.contains(SDLK_LEFT)) bunny_x[selectedBunny] -= speed * dt;
        if (keydown.contains(SDLK_RIGHT)) bunny_x[selectedBunny] += speed * dt;
        if (keydown.contains(SDLK_DOWN)) bunny_y[selectedBunny] -= speed * dt;
        if (keydown.contains(SDLK_UP)) bunny_y[selectedBunny] += speed * dt;

        const float scale = 0.5f;
        const float angle = time;
        const float cosA = std::cos(angle) * scale;
        const float sinA = std::sin(angle) * scale;

        const math::matrix4f models[] = {
            { // XY rotating
                cosA, -sinA, 0.f, bunny_x[0],
                sinA, cosA, 0.f, bunny_y[0],
                0.f, 0.f, scale, 0.f,
                0.f, 0.f, 0.f, 1.f,
            },
            { // XZ rotating
                cosA, 0.f, -sinA, bunny_x[1],
                0.f, scale, 0.f, bunny_y[1],
                sinA, 0.f, cosA, 0.f,
                0.f, 0.f, 0.f, 1.f,
            },
            { // YZ rotating
                scale, 0.f, 0.f, bunny_x[2],
                0.f, cosA, -sinA, bunny_y[2],
                0.f, sinA, cosA, 0.f,
                0.f, 0.f, 0.f, 1.f,
            },
        };

        const float cameraDistance = 3.f;
        math::matrix4f const view{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, -cameraDistance,
            0.f, 0.f, 0.f, 1.f,
        };

        const float near = 0.1f;
        const float far = 100.f;
        const float right = near;
        const float aspect = static_cast<float>(app.width()) / static_cast<float>(app.height());
        const float top = right / aspect;

        math::matrix4f const projection{
            near / right, 0.f, 0.f, 0.f,
            0.f, near / top, 0.f, 0.f,
            0.f, 0.f, far / (near - far), near * far / (near - far),
            0.f, 0.f, -1.f, 0.f,
        };

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.01, 0.02, 0.03, 1.0};

        WGPURenderPassDepthStencilAttachment depthAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depthAttachment.view = depthBufferView;
        depthAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthAttachment.depthStoreOp = WGPUStoreOp_Discard;
        depthAttachment.depthClearValue = 1.f;
        depthAttachment.depthReadOnly = false;

        WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        renderPassDescriptor.depthStencilAttachment = &depthAttachment;
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);

        auto viewProjectionTranspose = math::transpose(projection * view);
        wgpuRenderPassEncoderSetImmediates(renderPass, sizeof(math::matrix4f), &viewProjectionTranspose, sizeof(viewProjectionTranspose));

        wgpuRenderPassEncoderSetVertexBuffer(
            renderPass, 0, bunnyVertexBuffer.buffer, 0, bunnyVertexBuffer.bytes
        );

        wgpuRenderPassEncoderSetIndexBuffer(
            renderPass, bunnyIndexBuffer.buffer, WGPUIndexFormat_Uint32, 0, bunnyIndexBuffer.bytes
        );

        for (const auto& model : models) {
            auto modelTranspose = math::transpose(model);
            wgpuRenderPassEncoderSetImmediates(renderPass, 0, &modelTranspose, sizeof(modelTranspose));
            wgpuRenderPassEncoderDrawIndexed(
                renderPass,
                static_cast<std::uint32_t>(bunny.indices.size()),
                1, 0, 0, 0
            );
        }

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

    wgpuTextureViewRelease(depthBufferView);
    wgpuTextureRelease(depthBuffer);
    wgpuBufferRelease(bunnyIndexBuffer.buffer);
    wgpuBufferRelease(bunnyVertexBuffer.buffer);
    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);
} catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

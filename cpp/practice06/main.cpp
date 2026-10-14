#include <wgpu_app.hpp>
#include <file_utils.hpp>
#include <obj_loader.hpp>
#include <math/math.hpp>

#include <webgpu.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <unordered_set>

static std::filesystem::path const projectRoot = PROJECT_ROOT;
static constexpr uint32_t s_textureLayerCount = 4;

WGPUShaderModule createShaderModule(WGPUDevice device, std::filesystem::path const & path) {
    auto const source = loadFile(path);

    WGPUShaderSourceWGSL shaderSourceWGSL = WGPU_SHADER_SOURCE_WGSL_INIT;
    shaderSourceWGSL.code = {source.data(), source.size()};

    WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderModuleDescriptor.nextInChain = &shaderSourceWGSL.chain;

    return wgpuDeviceCreateShaderModule(device, &shaderModuleDescriptor);
}

template <typename T>
WGPUBuffer loadBuffer(WGPUDevice device, WGPUQueue queue, std::vector<T> const & data, WGPUBufferUsage usage) {
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.size = data.size() * sizeof(T);
    bufferDescriptor.usage = usage | WGPUBufferUsage_CopyDst;

    auto buffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    wgpuQueueWriteBuffer(queue, buffer, 0, data.data(), data.size() * sizeof(T));
    return buffer;
}

WGPUBindGroupLayout createTextureBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entries[2] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Fragment;
    entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[0].texture.sampleType = WGPUTextureSampleType_Float;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.entries = entries;
    descriptor.entryCount = 2;

    return wgpuDeviceCreateBindGroupLayout(device, &descriptor);
}

WGPURenderPipeline createMainPipeline(WGPUDevice device, WGPUShaderModule shaderModule, WGPUTextureFormat surfaceFormat) {
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

    WGPUVertexAttribute attributes[3] = {WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = 12;
    attributes[1].shaderLocation = 1;
    attributes[1].format = WGPUVertexFormat_Float32x3;

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.arrayStride = sizeof(ObjVertex);
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = attributes;
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    renderPipelineDescriptor.vertex.bufferCount = 1;
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.primitive.cullMode = WGPUCullMode_Back;
    renderPipelineDescriptor.fragment = &fragmentState;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

struct TextureImmediates {
    math::vector2f center;
    math::vector2f extent;
};

WGPURenderPipeline createTexturePipeline(WGPUDevice device, WGPUShaderModule shaderModule, WGPUTextureFormat surfaceFormat, WGPUBindGroupLayout textureBindGroupLayout) {
    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = sizeof(TextureImmediates);
    pipelineLayoutDescriptor.bindGroupLayouts = &textureBindGroupLayout;
    pipelineLayoutDescriptor.bindGroupLayoutCount = 1;

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.fragment = &fragmentState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

WGPUSampler createSampler(WGPUDevice device) {
    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    descriptor.addressModeU = WGPUAddressMode_ClampToEdge;
    descriptor.addressModeV = WGPUAddressMode_ClampToEdge;
    descriptor.addressModeW = WGPUAddressMode_ClampToEdge;
    descriptor.minFilter = WGPUFilterMode_Linear;
    descriptor.magFilter = WGPUFilterMode_Linear;
    descriptor.mipmapFilter = WGPUMipmapFilterMode_Linear;

    return wgpuDeviceCreateSampler(device, &descriptor);
}

WGPUTexture createTexture(WGPUDevice device, uint32_t width, uint32_t height) {
    WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
    extent.width = width;
    extent.height = height;
    extent.depthOrArrayLayers = s_textureLayerCount;

    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.format = WGPUTextureFormat_RGBA8Unorm;
    descriptor.size = extent;
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    descriptor.dimension = WGPUTextureDimension_2D;

    return wgpuDeviceCreateTexture(device, &descriptor);
}

WGPUBindGroup createBindGroup(WGPUDevice device, WGPUBindGroupLayout layout, WGPUTextureView textureView, WGPUSampler sampler) {
    WGPUBindGroupEntry entries[2];

    entries[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].textureView = textureView;

    entries[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].sampler = sampler;

    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.entryCount = 2;
    descriptor.entries = entries;
    descriptor.layout = layout;

    return wgpuDeviceCreateBindGroup(device, &descriptor);
}

int main() try {
    WgpuApp app("Practice06", 1280, 720, false);

    WGPUBindGroupLayout textureBindGroupLayout = createTextureBindGroupLayout(app.device());

    WGPUShaderModule mainShaderModule = createShaderModule(app.device(), projectRoot / "shader_main.wgsl");
    WGPURenderPipeline mainRenderPipeline = createMainPipeline(app.device(), mainShaderModule, WGPUTextureFormat_RGBA8Unorm);

    WGPUShaderModule textureShaderModule = createShaderModule(app.device(), projectRoot / "shader_texture.wgsl");
    WGPURenderPipeline textureRenderPipeline = createTexturePipeline(app.device(), textureShaderModule, app.surfaceFormat(), textureBindGroupLayout);

    ObjMesh dragon = loadObj(projectRoot / "dragon.obj");

    WGPUSampler dragonSampler = createSampler(app.device());
    WGPUTexture dragonTexture = nullptr;
    WGPUTextureView dragonTextureViews[s_textureLayerCount] = {};

    WGPUBindGroup bindGroups[s_textureLayerCount] = {};

    auto vertexBuffer = loadBuffer(app.device(), app.queue(), dragon.vertices, WGPUBufferUsage_Vertex);
    auto indexBuffer = loadBuffer(app.device(), app.queue(), dragon.indices, WGPUBufferUsage_Index);

    WGPUTexture depthBuffer = nullptr;
    WGPUTextureView depthBufferView = nullptr;

    auto lastFrameStart = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    float camera_distance = 2.f;
    float model_rotation = - std::numbers::pi_v<float> * 0.5f;

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

        const uint32_t newWidth = app.width() / 2;
        const uint32_t newHeight = app.height() / 2;

        const bool resized = !depthBuffer || wgpuTextureGetWidth(depthBuffer) != newWidth || wgpuTextureGetHeight(depthBuffer) != newHeight;

        if (resized) {
            for (auto bindGroup : bindGroups) {
                if (bindGroup) wgpuBindGroupRelease(bindGroup);
            }
            for (auto textureView : dragonTextureViews) {
                if (textureView) wgpuTextureViewRelease(textureView);
            }
            if (dragonTexture) wgpuTextureRelease(dragonTexture);

            dragonTexture = createTexture(app.device(), newWidth, newHeight);

            for (uint32_t i = 0; i < s_textureLayerCount; ++i) {
                WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
                descriptor.format = WGPUTextureFormat_RGBA8Unorm;
                descriptor.dimension = WGPUTextureViewDimension_2D;
                descriptor.baseMipLevel = 0;
                descriptor.mipLevelCount = 1;
                descriptor.baseArrayLayer = i;
                descriptor.arrayLayerCount = 1;

                dragonTextureViews[i] = wgpuTextureCreateView(dragonTexture, &descriptor);
                bindGroups[i] = createBindGroup(app.device(), textureBindGroupLayout, dragonTextureViews[i], dragonSampler);
            }
        }

        if (resized) {
            if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
            if (depthBuffer) wgpuTextureRelease(depthBuffer);

            WGPUTextureDescriptor depthBufferDescriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
            depthBufferDescriptor.usage = WGPUTextureUsage_RenderAttachment;
            depthBufferDescriptor.dimension = WGPUTextureDimension_2D;
            depthBufferDescriptor.size = {newWidth, newHeight, 1};
            depthBufferDescriptor.format = WGPUTextureFormat_Depth24Plus;
            depthBuffer = wgpuDeviceCreateTexture(app.device(), &depthBufferDescriptor);

            WGPUTextureViewDescriptor depthBufferViewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
            depthBufferViewDescriptor.format = WGPUTextureFormat_Depth24Plus;
            depthBufferViewDescriptor.dimension = WGPUTextureViewDimension_2D;
            depthBufferViewDescriptor.mipLevelCount = 1;
            depthBufferViewDescriptor.arrayLayerCount = 1;
            depthBufferViewDescriptor.aspect = WGPUTextureAspect_DepthOnly;
            depthBufferViewDescriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TransientAttachment;
            depthBufferView = wgpuTextureCreateView(depthBuffer, &depthBufferViewDescriptor);
        }

        auto const now = std::chrono::high_resolution_clock::now();
        float const dt = std::chrono::duration<float>(now - lastFrameStart).count();
        time += dt;
        lastFrameStart = now;

        if (keydown.contains(SDLK_LEFT)) model_rotation += 5.f * dt;
        if (keydown.contains(SDLK_RIGHT)) model_rotation -= 5.f * dt;
        if (keydown.contains(SDLK_UP)) camera_distance += 3.f * dt;
        if (keydown.contains(SDLK_DOWN)) camera_distance -= 3.f * dt;

        math::matrix4f const model = math::rotation_xz(model_rotation) * math::scale(math::vector{2.f, 2.f, 2.f});
        math::matrix4f const view = math::translation(math::vector{0.f, 0.f, - camera_distance});
        math::matrix4f const projection = math::perspective(std::numbers::pi_v<float> / 3.f, app.width() * 1.f / app.height(), 0.01f, 100.f);

        auto const modelTranspose = math::transpose(model);
        auto const viewProjectionTranspose = math::transpose(projection * view);

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = dragonTextureViews[0];
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.00, 0.06, 0.10, 1.0};

        WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depthStencilAttachment.view = depthBufferView;
        depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthStencilAttachment.depthStoreOp = WGPUStoreOp_Discard;
        depthStencilAttachment.depthClearValue = 1.f;
        depthStencilAttachment.depthReadOnly = WGPU_FALSE;

        WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        renderPassDescriptor.depthStencilAttachment = &depthStencilAttachment;
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, mainRenderPipeline);
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetImmediates(renderPass, 0, &modelTranspose, sizeof(modelTranspose));
        wgpuRenderPassEncoderSetImmediates(renderPass, 64, &viewProjectionTranspose, sizeof(viewProjectionTranspose));
        wgpuRenderPassEncoderDrawIndexed(renderPass, dragon.indices.size(), 1, 0, 0, 0);

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        // second render pass, just reusing same variables

        TextureImmediates const textureImmediates{
            .center = {-0.5f, -0.5f},
            .extent = {0.5f, 0.5f},
        };

        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        renderPassDescriptor.depthStencilAttachment = nullptr;
        renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, textureRenderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroups[0], 0, nullptr);
        wgpuRenderPassEncoderSetImmediates(renderPass, 0, &textureImmediates, sizeof(textureImmediates));
        wgpuRenderPassEncoderDraw(renderPass, 6, 1, 0, 0);

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        // submitting and releasing

        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(app.queue(), 1, &commandBuffer);
        wgpuCommandBufferRelease(commandBuffer);

        wgpuSurfacePresent(app.surface());

        wgpuTextureViewRelease(targetView);
        wgpuTextureRelease(surfaceTexture->texture);
    }

    if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
    if (depthBuffer) wgpuTextureRelease(depthBuffer);

    wgpuBufferRelease(vertexBuffer);
    wgpuBufferRelease(indexBuffer);

    for (auto bindGroup : bindGroups) {
        if (bindGroup) wgpuBindGroupRelease(bindGroup);
    }
    for (auto textureView : dragonTextureViews) {
        if (textureView) wgpuTextureViewRelease(textureView);
    }
    if (dragonTexture) wgpuTextureRelease(dragonTexture);
    wgpuSamplerRelease(dragonSampler);

    wgpuRenderPipelineRelease(textureRenderPipeline);
    wgpuShaderModuleRelease(textureShaderModule);

    wgpuRenderPipelineRelease(mainRenderPipeline);
    wgpuShaderModuleRelease(mainShaderModule);

    wgpuBindGroupLayoutRelease(textureBindGroupLayout);
} catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

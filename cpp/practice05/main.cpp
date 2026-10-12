#include <wgpu_app.hpp>
#include <file_utils.hpp>
#include <image_utils.hpp>
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

WGPURenderPipeline createPipeline(WGPUDevice device, WGPUShaderModule shaderModule,
                                  WGPUTextureFormat surfaceFormat, WGPUBindGroupLayout bindGroupLayout) {
    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
    pipelineLayoutDescriptor.bindGroupLayouts = &bindGroupLayout;

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPUVertexAttribute attributes[3] = {WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = 12;
    attributes[1].shaderLocation = 1;
    attributes[1].format = WGPUVertexFormat_Float32x3;
    attributes[2].shaderLocation = 2;
    attributes[2].format = WGPUVertexFormat_Float32x2;
    attributes[2].offset = offsetof(ObjVertex, texcoord); // 24

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.arrayStride = sizeof(ObjVertex);
    vertexBufferLayout.attributeCount = 3;
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

WGPUBuffer createUniformBuffer(WGPUDevice device, uint64_t bufferSize) {
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.size = bufferSize;
    bufferDescriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;

    return wgpuDeviceCreateBuffer(device, &bufferDescriptor);
}

WGPUTexture loadTexture(WGPUDevice device, WGPUQueue queue, const Image& image) {
    WGPUExtent3D extent3D = WGPU_EXTENT_3D_INIT;
    extent3D.width = image.width;
    extent3D.height = image.height;

    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.dimension = WGPUTextureDimension_2D;
    descriptor.format = WGPUTextureFormat_RGBA8Unorm;
    descriptor.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
    descriptor.size = extent3D;
    descriptor.mipLevelCount = 4;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &descriptor);

    WGPUTexelCopyTextureInfo texelCopyInfo = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    texelCopyInfo.texture = texture;
    texelCopyInfo.aspect = WGPUTextureAspect_All;
    texelCopyInfo.mipLevel = 0; // our real texture, biggest mip level

    WGPUTexelCopyBufferLayout copyLayout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    copyLayout.offset = 0;
    copyLayout.bytesPerRow = image.width * sizeof(Image::Pixel);
    copyLayout.rowsPerImage = image.height;

    size_t dataSize = copyLayout.bytesPerRow * copyLayout.rowsPerImage;

    wgpuQueueWriteTexture(queue, &texelCopyInfo, image.pixels.get(), dataSize, &copyLayout, &extent3D);

    Image::Pixel const mipColors[] = {
        {255, 0, 0, 255}, // red
        {0, 255, 0, 255}, // green
        {0, 0, 255, 255}, // blue
    };

    for (uint32_t level = 0; level < 3; ++level) {
        extent3D.width = std::max(1u, extent3D.width / 2);
        extent3D.height = std::max(1u, extent3D.height / 2);

        std::vector<Image::Pixel> pixels(extent3D.width * extent3D.height, mipColors[level]);

        // reusing
        texelCopyInfo.mipLevel = level + 1;
        copyLayout.bytesPerRow = extent3D.width * sizeof(Image::Pixel);
        copyLayout.rowsPerImage = extent3D.height;

        dataSize = copyLayout.bytesPerRow * copyLayout.rowsPerImage;

        wgpuQueueWriteTexture(queue, &texelCopyInfo, pixels.data(), dataSize, &copyLayout, &extent3D);
    }

    return texture;
}

WGPUSampler createTextureSampler(WGPUDevice device) {
    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    descriptor.addressModeU = WGPUAddressMode_Repeat;
    descriptor.addressModeV = WGPUAddressMode_Repeat;
    descriptor.magFilter = WGPUFilterMode_Nearest;
    descriptor.minFilter = WGPUFilterMode_Linear;
    descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;

    return wgpuDeviceCreateSampler(device, &descriptor);
}

WGPUBindGroupLayout createBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entries[3];

    entries[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Vertex;
    entries[0].buffer.type = WGPUBufferBindingType_Uniform;

    entries[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[1].texture.sampleType = WGPUTextureSampleType_Float;

    entries[2] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[2].binding = 2;
    entries[2].visibility = WGPUShaderStage_Fragment;
    entries[2].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor layoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.entryCount = 3;
    layoutDescriptor.entries = entries;

    return wgpuDeviceCreateBindGroupLayout(device, &layoutDescriptor);
}

WGPUBindGroup createBindGroup(WGPUDevice device, WGPUBindGroupLayout layout,
                              WGPUBuffer buffer, uint64_t bufferSize,
                              WGPUTextureView textureView, WGPUSampler sampler) {
    WGPUBindGroupEntry entries[3];

    entries[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].buffer = buffer;
    entries[0].offset = 0;
    entries[0].size = bufferSize;

    entries[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].textureView = textureView;

    entries[2] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[2].binding = 2;
    entries[2].sampler = sampler;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.entryCount = 3;
    bindGroupDescriptor.entries = entries;
    bindGroupDescriptor.layout = layout;

    return wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
}

int main() try {
    WgpuApp app("Practice05", 720, 540, false);

    uint64_t bufferSize = 2 * (4 * 4) * sizeof(float); // 2 matrices 4x4
    WGPUBuffer buffer = createUniformBuffer(app.device(), bufferSize);
    WGPUBindGroupLayout bindGroupLayout = createBindGroupLayout(app.device());

    Image cowImage = loadImage(projectRoot / "cow.png");
    WGPUTexture cowTexture = loadTexture(app.device(), app.queue(), cowImage);
    WGPUTextureView cowTextureView = wgpuTextureCreateView(cowTexture, nullptr);
    WGPUSampler sampler = createTextureSampler(app.device());

    WGPUBindGroup bindGroup = createBindGroup(app.device(), bindGroupLayout,
                                              buffer, bufferSize,
                                              cowTextureView, sampler);

    WGPUShaderModule shaderModule = createShaderModule(app.device(), projectRoot / "shader.wgsl");
    WGPURenderPipeline renderPipeline = createPipeline(app.device(), shaderModule, app.surfaceFormat(), bindGroupLayout);

    wgpuBindGroupLayoutRelease(bindGroupLayout);

    ObjMesh cow = loadObj(projectRoot / "cow.obj");
    auto vertexBuffer = loadBuffer(app.device(), app.queue(), cow.vertices, WGPUBufferUsage_Vertex);
    auto indexBuffer = loadBuffer(app.device(), app.queue(), cow.indices, WGPUBufferUsage_Index);

    WGPUTexture depthBuffer = nullptr;
    WGPUTextureView depthBufferView = nullptr;

    auto lastFrameStart = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    float camera_distance = 3.f;
    float model_rotation = std::numbers::pi_v<float> * 0.75f;

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

        if (!depthBuffer || wgpuTextureGetWidth(depthBuffer) != app.width() || wgpuTextureGetHeight(depthBuffer) != app.height()) {
            if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
            if (depthBuffer) wgpuTextureRelease(depthBuffer);

            WGPUTextureDescriptor depthBufferDescriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
            depthBufferDescriptor.usage = WGPUTextureUsage_RenderAttachment;
            depthBufferDescriptor.dimension = WGPUTextureDimension_2D;
            depthBufferDescriptor.size = {(std::uint32_t)app.width(), (std::uint32_t)app.height(), 1};
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

        math::matrix4f const model = math::rotation_xz(model_rotation);
        math::matrix4f const view = math::translation(math::vector{0.f, 0.f, - camera_distance});
        math::matrix4f const projection = math::perspective(std::numbers::pi_v<float> / 3.f, app.width() * 1.f / app.height(), 0.01f, 100.f);

        auto modelTranspose = math::transpose(model);
        auto viewProjectionTranspose = math::transpose(projection * view);

        wgpuQueueWriteBuffer(app.queue(), buffer, 0, &modelTranspose, sizeof(modelTranspose));
        wgpuQueueWriteBuffer(app.queue(), buffer, 64, &viewProjectionTranspose, sizeof(viewProjectionTranspose));

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.6, 0.8, 1.0, 1.0};

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

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);

        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, nullptr);

        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);

        wgpuRenderPassEncoderDrawIndexed(renderPass, cow.indices.size(), 1, 0, 0, 0);

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

    if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
    if (depthBuffer) wgpuTextureRelease(depthBuffer);

    wgpuBufferRelease(vertexBuffer);
    wgpuBufferRelease(indexBuffer);

    wgpuSamplerRelease(sampler);
    wgpuTextureViewRelease(cowTextureView);
    wgpuTextureRelease(cowTexture);
    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupRelease(bindGroup);
    wgpuBufferRelease(buffer);
} catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

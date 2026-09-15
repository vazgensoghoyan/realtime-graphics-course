#include <wgpu_app.hpp>
#include <file_utils.hpp>

#include <webgpu.h>

#include <exception>
#include <filesystem>
#include <iostream>

namespace {

    std::filesystem::path const projectRoot = PROJECT_ROOT;

    WGPUStringView toWGPUStringView(std::string_view str) {
        return {str.data(), str.size()};
    }

    WGPUShaderModule initShaderModule(WgpuApp& app, std::string_view shaderName) {
        const std::string shaderCodeText = loadFile(projectRoot / shaderName);

        WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
        shaderSource.code = toWGPUStringView(shaderCodeText);

        WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        shaderModuleDescriptor.nextInChain = &shaderSource.chain;

        return wgpuDeviceCreateShaderModule(app.device(), &shaderModuleDescriptor);
    }

    WGPURenderPipeline initRenderPipeline(WgpuApp& app, WGPUShaderModule shaderModule) {
        WGPUVertexState vertexState = WGPU_VERTEX_STATE_INIT;
        vertexState.module = shaderModule;
        vertexState.entryPoint = toWGPUStringView("vertexMain");

        WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
        colorTarget.format = app.surfaceFormat();
        colorTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
        fragmentState.module = shaderModule;
        fragmentState.entryPoint = toWGPUStringView("fragmentMain");
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTarget;

        WGPUPrimitiveState primitiveState = WGPU_PRIMITIVE_STATE_INIT;
        primitiveState.topology = WGPUPrimitiveTopology_TriangleList;

        WGPURenderPipelineDescriptor pipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pipelineDescriptor.vertex = vertexState;
        pipelineDescriptor.fragment = &fragmentState;
        pipelineDescriptor.primitive = primitiveState;

        return wgpuDeviceCreateRenderPipeline(
            app.device(),
            &pipelineDescriptor
        );
    }

} // namespace

int main() try {
    WgpuApp app("Practice01", 1280, 720, false);

    WGPUShaderModule shaderModule = initShaderModule(app, "shader.wgsl");
    WGPURenderPipeline renderPipeline = initRenderPipeline(app, shaderModule);

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
            }
        }

        std::optional<WGPUSurfaceTexture> surfaceTexture = app.beginFrame();
        if (!surfaceTexture) {
            continue;
        }

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        // Frame rendering code goes here

        WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.15, 0.4, 0.55, 1.0};

        WGPURenderPassDescriptor passDescriptor{};
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &colorAttachment;

        WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, renderPipeline);
        wgpuRenderPassEncoderDraw(renderPassEncoder, /*vertexCount=*/3, /*instanceCount=*/1, /*firstVertex=*/0, /*firstInstance=*/0);

        wgpuRenderPassEncoderEnd(renderPassEncoder);

        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(commandEncoder, nullptr);
        wgpuQueueSubmit(app.queue(), 1, &commandBuffer);

        wgpuRenderPassEncoderRelease(renderPassEncoder);
        wgpuCommandBufferRelease(commandBuffer);
        wgpuCommandEncoderRelease(commandEncoder);

        // to here

        wgpuTextureViewRelease(targetView);

        wgpuSurfacePresent(app.surface());
        wgpuTextureRelease(surfaceTexture->texture);
    }

    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);

} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

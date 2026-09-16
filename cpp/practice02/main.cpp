#include <wgpu_app.hpp>
#include <file_utils.hpp>

#include <webgpu.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <cmath>

namespace {

    static std::filesystem::path const projectRoot = PROJECT_ROOT;

    WGPUShaderModule createShaderModule(WGPUDevice device, std::filesystem::path const& path) {
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
        pipelineLayoutDescriptor.immediateSize = 128; // 2 matrices 4x4 of floats

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

    void handleMoving(std::unordered_set<SDL_Keycode>& keydown, float& xOffset, float& yOffset) {
        float offset = 0.01f;
        if (keydown.contains(SDLK_F)) { // dont know why, SDLK_BACKSPACE didnt work :(
            offset *= 2;
        }

        if (keydown.contains(SDLK_LEFT)) {
            xOffset -= offset;
        }
        if (keydown.contains(SDLK_RIGHT)) {
            xOffset += offset;
        }
        if (keydown.contains(SDLK_DOWN)) {
            yOffset -= offset;
        }
        if (keydown.contains(SDLK_UP)) {
            yOffset += offset;
        }
    }

} // namespace

int main() try {
    WgpuApp app("Practice02", 1280, 720, false);

    WGPUShaderModule shaderModule = createShaderModule(app.device(), projectRoot / "shader.wgsl");
    WGPURenderPipeline renderPipeline = createPipeline(app.device(), shaderModule, app.surfaceFormat());

    auto lastFrameStart = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    std::unordered_set<SDL_Keycode> keydown;

    float xOffset = 0;
    float yOffset = 0;

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

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.07, 0.21, 0.30, 1.0};

        WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);

        // setting immediates (for now only 'scale')
        constexpr float scale = 0.3f;
        const float angle = time;

        const float cosScaled = std::cos(angle) * scale;
        const float sinScaled = std::sin(angle) * scale;

        handleMoving(keydown, xOffset, yOffset);

        const float matrixTransform[16] = {
            cosScaled, -sinScaled, 0, xOffset,
            sinScaled, cosScaled, 0, yOffset,
            0, 0, 0, 0,
            0, 0, 0, 1,
        };

        const int width = app.width();
        const int height = app.height();
        const float aspectRatio = static_cast<float>(height) / width;

        const float matrixView[16] = {
            aspectRatio, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1,
        };

        wgpuRenderPassEncoderSetImmediates(renderPass, 0, &matrixTransform, sizeof(matrixTransform));
        wgpuRenderPassEncoderSetImmediates(renderPass, sizeof(matrixTransform), &matrixView, sizeof(matrixView));
        // immediates setting is done

        wgpuRenderPassEncoderDraw(renderPass, 18, 1, 0, 0);
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

    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);
} catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

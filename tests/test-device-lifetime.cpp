#include "testing.h"
#include <slang-rhi.h>
#include "core/smart-pointer.h"
#include "rhi-shared.h"
#include "device.h"

using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("device-lifetime", ALL | DontCreateDevice)
{
    // Create a device
    DeviceDesc deviceDesc = {};
    deviceDesc.deviceType = ctx->deviceType;
    deviceDesc.adapter = getSelectedDeviceAdapter(ctx->deviceType);
    ComPtr<IDevice> testDevice;
    REQUIRE_CALL(getRHI()->createDevice(deviceDesc, testDevice.writeRef()));

    Device* devicePtr = static_cast<Device*>(testDevice.get());

    // Create a buffer
    ComPtr<IBuffer> buffer;
    BufferDesc bufferDesc = {};
    bufferDesc.size = 1024;
    bufferDesc.usage = BufferUsage::ShaderResource;
    REQUIRE_CALL(testDevice->createBuffer(bufferDesc, nullptr, buffer.writeRef()));
    uint64_t deviceRefCountBuffer = devicePtr->getReferenceCount();

    // Create a texture
    ComPtr<ITexture> texture;
    TextureDesc textureDesc = {};
    textureDesc.format = Format::RGBA32Float;
    textureDesc.usage = TextureUsage::ShaderResource;
    REQUIRE_CALL(testDevice->createTexture(textureDesc, nullptr, texture.writeRef()));
    uint64_t deviceRefCountTexture = devicePtr->getReferenceCount();

    // Create a sampler
    ComPtr<ISampler> sampler;
    SamplerDesc samplerDesc = {};
    REQUIRE_CALL(testDevice->createSampler(samplerDesc, sampler.writeRef()));
    uint64_t deviceRefCountSampler = devicePtr->getReferenceCount();

    // Create acceleration structure
    ComPtr<IAccelerationStructure> accelerationStructure;
    if (testDevice->hasFeature(Feature::AccelerationStructure))
    {
        AccelerationStructureDesc accelerationStructureDesc = {};
        accelerationStructureDesc.size = 1024;
        REQUIRE_CALL(
            testDevice->createAccelerationStructure(accelerationStructureDesc, accelerationStructure.writeRef())
        );
    }
    uint64_t deviceRefCountAccelerationStructure = devicePtr->getReferenceCount();

    // Create fence
    ComPtr<IFence> fence;
    if (testDevice->getDeviceType() != DeviceType::D3D11)
    {
        FenceDesc fenceDesc = {};
        REQUIRE_CALL(testDevice->createFence(fenceDesc, fence.writeRef()));
    }
    uint64_t deviceRefCountFence = devicePtr->getReferenceCount();

    testDevice.setNull();

    CHECK(devicePtr->getReferenceCount() == deviceRefCountFence - 1);
    fence.setNull();

    CHECK(devicePtr->getReferenceCount() == deviceRefCountAccelerationStructure - 1);
    accelerationStructure.setNull();

    CHECK(devicePtr->getReferenceCount() == deviceRefCountSampler - 1);
    sampler.setNull();

    CHECK(devicePtr->getReferenceCount() == deviceRefCountTexture - 1);
    texture.setNull();

    CHECK(devicePtr->getReferenceCount() == deviceRefCountBuffer - 1);
    buffer.setNull();
}

GPU_TEST_CASE("multi-queue-device-lifetime", D3D12 | DontCreateDevice)
{
    DeviceDesc deviceDesc = {};
    deviceDesc.deviceType = ctx->deviceType;
    deviceDesc.adapter = getSelectedDeviceAdapter(ctx->deviceType);
    ComPtr<IDevice> testDevice;
    REQUIRE_CALL(getRHI()->createDevice(deviceDesc, testDevice.writeRef()));

    static constexpr const char* source = R"(
        RWStructuredBuffer<float> buffer;
        float value;
        [shader("compute")]
        [numthreads(1, 1, 1)]
        void computeMain(uint3 id : SV_DispatchThreadID)
        {
            buffer[id.x] = value;
        }
    )";
    ComPtr<IShaderProgram> shaderProgram;
    REQUIRE_CALL(loadComputeProgramFromSource(
        testDevice, source, shaderProgram.writeRef()));
    ComputePipelineDesc pipelineDesc = {};
    pipelineDesc.program = shaderProgram;
    ComPtr<IComputePipeline> pipeline;
    REQUIRE_CALL(testDevice->createComputePipeline(pipelineDesc, pipeline.writeRef()));

    BufferDesc bufferDesc = {};
    bufferDesc.size = 4 * sizeof(float);
    bufferDesc.elementSize = sizeof(float);
    bufferDesc.usage = BufferUsage::ShaderResource | BufferUsage::UnorderedAccess;
    bufferDesc.defaultState = ResourceState::UnorderedAccess;
    ComPtr<IBuffer> buffer;
    REQUIRE_CALL(testDevice->createBuffer(bufferDesc, nullptr, buffer.writeRef()));

    {
        auto queue = testDevice->getQueue(QueueType::Compute);
        auto commandEncoder = queue->createCommandEncoder();
        auto passEncoder = commandEncoder->beginComputePass();
        auto rootObject = passEncoder->bindPipeline(pipeline);
        ShaderCursor cursor(rootObject);
        cursor["buffer"].setBinding(buffer);
        cursor["value"].setData(1.0f);
        passEncoder->dispatchCompute(1, 1, 1);
        passEncoder->end();
        queue->submit(commandEncoder->finish());
        queue->waitOnHost();
    }

    buffer.setNull();
    pipeline.setNull();
    shaderProgram.setNull();
    testDevice.setNull();
}

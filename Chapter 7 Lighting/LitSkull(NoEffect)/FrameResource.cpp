#include "FrameResource.h"

FrameResource::FrameResource(ID3D11Device * device, UINT passCount, UINT objectCount, UINT materialCount)
{
    PassCB = std::make_unique<ConstantBuffer<PassConstants>>(device, passCount);
    MaterialCB = std::make_unique<ConstantBuffer<MaterialConstants>>(device, materialCount);
    ObjectCB = std::make_unique<ConstantBuffer<ObjectConstants>>(device, objectCount);
}

FrameResource::~FrameResource()
{
}


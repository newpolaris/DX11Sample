#include "FrameResource.h"

FrameResource::FrameResource(ID3D11Device * device, UINT passCount, UINT objectCount, UINT materialCount)
{
    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount);
    // MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount);
}

FrameResource::~FrameResource()
{
}


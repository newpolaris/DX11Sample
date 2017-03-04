#include "FrameResource.h"

FrameResource::FrameResource(ID3D11Device * device, UINT passCount, UINT objectCount, UINT materialCount)
{
    // PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    // MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount);
}

FrameResource::~FrameResource()
{
}

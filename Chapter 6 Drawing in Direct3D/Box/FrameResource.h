#pragma once

#include "../../Common/d3dUtil.h"
#include "../../Common/UploadBuffer.h"

struct ObjectConstants
{
	XMMATRIX worldViewProj;
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource
{
public:
    
    FrameResource(ID3D11Device* device, UINT passCount, UINT objectCount, UINT materialCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    // std::unique_ptr<UploadBuffer<FrameConstants>> FrameCB = nullptr;
    // std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    // std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
};
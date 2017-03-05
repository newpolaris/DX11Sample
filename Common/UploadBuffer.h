#pragma once
#pragma once

#include "d3dUtil.h"

template<typename T>
class UploadBuffer
{
public:
    UploadBuffer(ID3D11Device* device, UINT elementCount, D3D11_SUBRESOURCE_DATA* data = nullptr) 
    {
        mElementByteSize = sizeof(T);

		mUploadBuffer = d3dHelper::CreateConstantBuffer(
			device, mElementByteSize);

		mMappedData.assign(elementCount*mElementByteSize, 0);
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer() {}

    ID3D11Buffer* Resource() const
    {
        return static_cast<ID3D11Buffer*>(mUploadBuffer.Get());
    }

    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&mMappedData[elementIndex*mElementByteSize], &data, sizeof(T));
    }

	void UploadData(ID3D11DeviceContext* context, int elementIndex)
	{
		D3D11_MAPPED_SUBRESOURCE Data;
		Data.pData = nullptr;
		Data.DepthPitch = Data.RowPitch = 0;

		const int subresourceID = 0;

		auto pBuffer = mUploadBuffer.Get();
		ThrowIfFailed(context->Map(
			pBuffer,
			subresourceID,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&Data));
		memcpy(Data.pData, &mMappedData[elementIndex*mElementByteSize], sizeof(T));
		context->Unmap(pBuffer, subresourceID);
	}

private:
    Microsoft::WRL::ComPtr<ID3D11Resource> mUploadBuffer;
    std::vector<BYTE> mMappedData;
    UINT mElementByteSize = 0, mElementCount = 0;
};

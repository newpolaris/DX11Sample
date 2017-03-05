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
		mElementCount = elementCount;

		mUploadBuffer = d3dHelper::CreateStructuredBuffer(
			device, elementCount, mElementByteSize, (data == nullptr), false, data);

		for (UINT i = 0; i < elementCount; i++)
			mView.emplace_back(d3dHelper::CreateSRV(device, mUploadBuffer.Get(), i, 1));

		mMappedData.assign(elementCount*mElementByteSize, 0);
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer() {}

    ID3D11Resource* Resource() const
    {
        return mUploadBuffer.Get();
    }
	ID3D11ShaderResourceView* GetView(int index) {
		return mView[index].Get();
	}

    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&mMappedData[elementIndex*mElementByteSize], &data, sizeof(T));
    }

	void UploadData(ID3D11DeviceContext* context)
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
		memcpy(Data.pData, mMappedData.data(), mElementCount*sizeof(T));
		context->Unmap(pBuffer, subresourceID);
	}

private:
    Microsoft::WRL::ComPtr<ID3D11Resource> mUploadBuffer;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mView;
    std::vector<BYTE> mMappedData;
    UINT mElementByteSize = 0, mElementCount = 0;
};

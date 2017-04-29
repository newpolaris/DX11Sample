#pragma once
#pragma once

#include "d3dUtil.h"

template<typename T>
class ConstantBuffer
{
public:
    ConstantBuffer(ID3D11Device* device, UINT elementCount) 
    {
        mElementByteSize = sizeof(T);

		for (UINT i = 0; i < elementCount; i++)
			mUploadBuffer.emplace_back(d3dHelper::CreateConstantBuffer(device, mElementByteSize));
		mBackup.resize(elementCount);
    }

    ConstantBuffer(const ConstantBuffer& rhs) = delete;
    ConstantBuffer& operator=(const ConstantBuffer& rhs) = delete;
    ~ConstantBuffer() {}

	T& backup(int elementIndex) 
	{
		return mBackup[elementIndex];
	}

    ID3D11Buffer* Resource(int elementIndex) const
    {
        return static_cast<ID3D11Buffer*>(mUploadBuffer[elementIndex].Get());
    }

	void UploadData(ID3D11DeviceContext* context, int elementIndex, const T& data)
	{
		mBackup[elementIndex] = data;

		D3D11_MAPPED_SUBRESOURCE Data;
		Data.pData = nullptr;
		Data.DepthPitch = Data.RowPitch = 0;

		const int subresourceID = 0;

		auto pBuffer = mUploadBuffer[elementIndex].Get();
		ThrowIfFailed(context->Map(
			pBuffer,
			subresourceID,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&Data));
		memcpy(Data.pData, &data, sizeof(T));
		context->Unmap(pBuffer, subresourceID);
	}

private:
    std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> mUploadBuffer;
	std::vector<T> mBackup;
    UINT mElementByteSize = 0, mElementCount = 0;
};

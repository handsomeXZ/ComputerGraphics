#pragma once
#include "d3dUtil.h"

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* pdevice, UINT elementCount, bool isConstantBuffer)
		: misConstantBuffer(isConstantBuffer)
	{
		if (isConstantBuffer) {
			mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));
		}
		else {
			mElementByteSize = sizeof(T);
		}
		CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC&& desc = CD3DX12_RESOURCE_DESC::Buffer(1 * mElementByteSize);
		pdevice->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mUploadBuffer.GetAddressOf())
		);

		mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData));
	};
	UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer() {
		if (mMappedData != nullptr) {
			mUploadBuffer->Unmap(0, nullptr);
		}
		mMappedData = nullptr;
	};

	ID3D12Resource* Get() {
		return mUploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) {
		memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
	}


private:
	BYTE* mMappedData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	UINT mElementByteSize = 0;
	bool misConstantBuffer = false;
};


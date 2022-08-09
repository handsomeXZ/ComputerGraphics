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

		pdevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1 * mElementByteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mUploadBuffer.GetAddressOf())
		);

		mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData))
	};
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


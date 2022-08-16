#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* pdevice, UINT passCount, UINT objectCount, UINT materialCount) {

	pdevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CmdListAlloc.GetAddressOf()));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(pdevice, passCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(pdevice, objectCount, true);
	MatCB = std::make_unique<UploadBuffer<MaterialConstants>>(pdevice, materialCount, true);
}

FrameResource::~FrameResource(){
}
#include "d3dUtil.h"
#include <comdef.h>


Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
    ID3D12Device* pdevice,
    ID3D12GraphicsCommandList* pcmdList,
    const void* initData,
    UINT64 ByteSize,
    _Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
) 
{
    Microsoft::WRL::ComPtr<ID3D12Resource> pdefualtBuffer;
    
    // 此处利用 d3dx12.h 提供的帮助函数
    // 创建默认缓冲区资源
    pdevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(ByteSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(pdefualtBuffer.GetAddressOf())
    );

    // 为了将CPU端内存中的数据复制到默认缓冲区，我们还需要创建一个处于中介位置上的上传堆
    pdevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(ByteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())
    );

    // 描述希望复制到默认缓冲区资源的数据
    D3D12_SUBRESOURCE_DATA psubresourceData = {};
    psubresourceData.pData = initData;
    psubresourceData.RowPitch = ByteSize;
    psubresourceData.RowPitch = psubresourceData.RowPitch;

    // 将数据复制到默认缓冲区
    // UpdateSubresources 辅助函数会将数据从CPU端的内存中复制到位于中介位置的上传堆里接着
    // UpdateSubresources 有三种实现，这里采用的是先在栈中分配复制过程所需的内存，再实现将资源从上传堆复制到默认堆。
    // 再通过调用 CopySubresourceRegion 函数，把上传堆内的数据复制到 mBuffer中
    // 上传堆内的数据最终会被复制到 默认堆中，而不是 mBuffer 中
    pcmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(uploadBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        )
    );

    UpdateSubresources<1>(
        pcmdList,
        pdefualtBuffer.Get(),
        uploadBuffer.Get(),
        0, 0, 1, &psubresourceData
        );

    pcmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(uploadBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ
        )
    );

    return pdefualtBuffer;
}
Microsoft::WRL::ComPtr<ID3DBlob> d3dUtil::CompileShader(const std::wstring& filename,
    const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
    UINT compileFlags = 0;
    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(),
        target.c_str(), compileFlags, 0, &byteCode, &errors);

    ThrowIfFailed(hr);
    return byteCode;
}

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

std::wstring DxException::ToString()const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}
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
    
    // �˴����� d3dx12.h �ṩ�İ�������
    // ����Ĭ�ϻ�������Դ
    CD3DX12_HEAP_PROPERTIES defaultproperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC&& defaultdesc = CD3DX12_RESOURCE_DESC::Buffer(ByteSize);
    pdevice->CreateCommittedResource(
        &defaultproperties,
        D3D12_HEAP_FLAG_NONE,
        &defaultdesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(pdefualtBuffer.GetAddressOf())
    );

    // Ϊ�˽�CPU���ڴ��е����ݸ��Ƶ�Ĭ�ϻ����������ǻ���Ҫ����һ�������н�λ���ϵ��ϴ���
    CD3DX12_HEAP_PROPERTIES uploadproperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC&& uploaddesc = CD3DX12_RESOURCE_DESC::Buffer(ByteSize);
    pdevice->CreateCommittedResource(
        &uploadproperties,
        D3D12_HEAP_FLAG_NONE,
        &uploaddesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())
    );

    // ����ϣ�����Ƶ�Ĭ�ϻ�������Դ������
    D3D12_SUBRESOURCE_DATA psubresourceData = {};
    psubresourceData.pData = initData;
    psubresourceData.RowPitch = ByteSize;
    psubresourceData.RowPitch = psubresourceData.RowPitch;

    // �����ݸ��Ƶ�Ĭ�ϻ�����
    // UpdateSubresources ���������Ὣ���ݴ�CPU�˵��ڴ��и��Ƶ�λ���н�λ�õ��ϴ��������
    // UpdateSubresources ������ʵ�֣�������õ�������ջ�з��临�ƹ���������ڴ棬��ʵ�ֽ���Դ���ϴ��Ѹ��Ƶ�Ĭ�϶ѡ�
    // ��ͨ������ CopySubresourceRegion ���������ϴ����ڵ����ݸ��Ƶ� mBuffer��
    // �ϴ����ڵ��������ջᱻ���Ƶ� Ĭ�϶��У������� mBuffer ��
    CD3DX12_RESOURCE_BARRIER&& pbarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(pdefualtBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    pcmdList->ResourceBarrier(1, &pbarrier1);

    UpdateSubresources<1>(
        pcmdList,
        pdefualtBuffer.Get(),
        uploadBuffer.Get(),
        0, 0, 1, &psubresourceData
        );
    CD3DX12_RESOURCE_BARRIER pbarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(pdefualtBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    pcmdList->ResourceBarrier(1, &pbarrier2);

    return pdefualtBuffer;
}
Microsoft::WRL::ComPtr<ID3DBlob> d3dUtil::CompileShader(const std::wstring& filename,
    const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(),
        target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

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
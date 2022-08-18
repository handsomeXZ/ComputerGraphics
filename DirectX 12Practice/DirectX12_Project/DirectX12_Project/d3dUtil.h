#pragma once

#include "windows.h"
#include "cassert"
#include "wrl.h"
#include "string"
#include "d3d12.h"
#include "dxgi1_4.h"
#include "DirectXMath.h"
#include "DirectXColors.h"
#include "MathHelper.h"
#include "memory"
#include "d3dx12.h"
#include "d3dcompiler.h"
#include "vector"
#include "DirectXCollision.h"
#include "unordered_map"
#include "array"

#define MAX_LIGHTS 10


extern const int gNumFrameResource;


inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class d3dUtil
{
public:
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* pdevice,
        ID3D12GraphicsCommandList* pcmdList,
        const void* initData,
        UINT64 ByteSize,
        _Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
    );
    
    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& filename,
        const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);


};

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

struct SubmeshGeometry
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;

    // Bounding box of the geometry defined by this submesh. 
    // This is used in later chapters of the book.
    DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
    // Give it a name so we can look it up by name.
    std::string Name;

    // System memory copies.  Use Blobs because the vertex/index format can be generic.
    // It is up to the client to cast appropriately.  
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // Data about the buffers.
    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    // A MeshGeometry may store multiple geometries in one vertex/index buffer.
    // Use this container to define the Submesh geometries so we can draw
    // the Submeshes individually.
    
    
    //std::unordered_map<std::string, SubmeshGeometry> DrawArgs;
    std::vector<SubmeshGeometry> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStride;
        vbv.SizeInBytes = VertexBufferByteSize;

        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;

        return ibv;
    }

    // We can free this memory after we finish upload to the GPU.
    void DisposeUploaders()
    {
        VertexBufferUploader = nullptr;
        IndexBufferUploader = nullptr;
    }
};
struct Material {
    std::string Name;

    int MatCBIndex = -1;

    int DiffuseSrvHeapIndex = -1;

    int NumFramesDirty = gNumFrameResource;

    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.0f,0.0f,0.0f };
    float Routhness = 0.0f;
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};
struct MaterialConstants {

    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.0f,0.0f,0.0f };
    float Routhness = 0.0f;

    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};
struct Light {
    DirectX::XMFLOAT3 Strength = { 1.0f,1.0f,1.0f };
    float Falloffstart = 1.0f;  // 仅供点光源、聚光灯使用
    DirectX::XMFLOAT3 Direction = { 0.0f,-1.0f,0.0f };  // 仅供方向光源、聚光灯使用
    float Falloffend = 10.0f;  // 仅供点光源、聚光灯使用
    DirectX::XMFLOAT3 Position = { 0.0f,0.0f,0.0f };    // 仅供点光源、聚光灯使用
    float SpotPower = 64.0f;    // 仅供点光源使用
};
struct Texture
{
    std::string Name;

    std::wstring Filename;

    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};
#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif
#pragma once

#include "../d3dApp.h"
#include "../UploadBuffer.h"

struct Vertex {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 COlor;
};

struct ObjectConstants {
    DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class Box :
    public D3DApp
{
public:
    Box(HINSTANCE hInstance);
    Box(const Box& rhs) = delete;
    Box& operator=(const Box& rhs) = delete;
    ~Box();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update()override;
    virtual void Draw()override;

    void BuildVertexBuffers();
    void BuildIndexBuffer();
    void BuildCBuffer();
    void BuildRootSigantureAndDescriptorTable();
    void BuildShadersAndInputLayout();
    void BuildPSO();
    void BuildDescriptorHeaps();

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexUploadBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mIndexUploadBuffer = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;


    DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * DirectX::XM_PI;
    float mPhi = DirectX::XM_PIDIV4;
    float mRadius = 5.0f;

    D3D12_VERTEX_BUFFER_VIEW Vbv = {};
    D3D12_INDEX_BUFFER_VIEW Ibv = {};
};


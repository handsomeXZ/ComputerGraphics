#pragma once

#include "../d3dApp.h"
#include "../UploadBuffer.h"
#include "../FrameResource.h"
#include "../GeometryGenerator.h"


// 每个物体绘制所需的参数集合
struct RenderItem {
    RenderItem() = default;

    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    int NumFramesDirty = gNumFrameResource;

    // GPU 常量缓冲区中，当前渲染项中的，物体常量缓冲区
    UINT ObjectCBIndex = -1;

    D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    // DrawIndexedInstanced
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    UINT BaseVertexLocation = 0;
};


class MultiGeo :
    public D3DApp
{
public:
    MultiGeo(HINSTANCE hInstance);
    MultiGeo(const MultiGeo& rhs) = delete;
    MultiGeo& operator=(const MultiGeo& rhs) = delete;
    ~MultiGeo();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update()override;
    virtual void Draw()override;


    void UpdateCamera();
    void OnKeyboardInput();
    void UpdateObjectCBs();
    void UpdateMainPassCB();
    void UpdateMaterials();

    void LoadTexture();
    void BuildMaterials();
    void BuildRootSigantureAndDescriptorTable();
    void BuildDescriptorHeaps();
    void BuildCBufferView();
    void BuildShaderView();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSO();
    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

    bool mIsWireframe = false;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSO;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

    DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };

    float mTheta = 1.3f * DirectX::XM_PI;
    float mPhi = 0.4f * DirectX::XM_PI;
    float mRadius = 2.5f;
    


    // FrameResource
    UINT mCurrentFrameResourceIndex = 0;
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrentFrameResource = nullptr;
    PassConstants mMainPassCB;
    
    // RenderItem
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::vector<RenderItem*> mOpaqueRitems;
    std::vector<RenderItem*> mTransparentRitems;

    // Texture
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;

};


#include "Box.h"

Box::Box(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

Box::~Box()
{
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        Box theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

void Box::BuildVertexBuffers() {
    Vertex vertexs[] = {
        { DirectX::XMFLOAT3(-1.0f,-1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::White) },
        { DirectX::XMFLOAT3(+1.0f,-1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::Black) },
        { DirectX::XMFLOAT3(+1.0f,+1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::Red) },
        { DirectX::XMFLOAT3(+1.0f,+1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Green) },
        { DirectX::XMFLOAT3(-1.0f,+1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::Blue) },
        { DirectX::XMFLOAT3(-1.0f,+1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Yellow) },
        { DirectX::XMFLOAT3(-1.0f,-1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Cyan) },
        { DirectX::XMFLOAT3(+1.0f,-1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Magenta) },
    };

    const UINT64 vbByteSize = 8 * sizeof(Vertex);
    mVertexBuffer = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertexs, vbByteSize, mVertexUploadBuffer);

    D3D12_VERTEX_BUFFER_VIEW Vbv = {};
    Vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    Vbv.SizeInBytes = vbByteSize;
    Vbv.StrideInBytes = sizeof(Vertex);

    mCommandList->IASetVertexBuffers(0,1,&Vbv);

}

void Box::BuildIndexBuffer() {
    std::uint16_t indices[] = {
        // 正面
        0,4,2,
        0,2,1,

        // 背面
        6,3,5,
        6,7,3,

        // 左面
        6,5,4,
        6,4,0,

        // 右面
        1,2,3,
        1,3,7,

        // 顶面
        4,5,3,
        4,3,2,

        // 底面
        6,0,1,
        6,1,7
    };

    const UINT64 ibByteSize = 36 * sizeof(std::uint16_t);
    mIndexBuffer = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices, ibByteSize, mIndexUploadBuffer);

    D3D12_INDEX_BUFFER_VIEW Ibv = {};
    Ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    Ibv.SizeInBytes = ibByteSize;
    Ibv.Format = DXGI_FORMAT_R16_UINT;
    mCommandList->IASetIndexBuffer(&Ibv);


}

void Box::BuildCBuffer() {
    ObjectCBuffer = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
    // 因为只要绘制一个物体，所以创建一个存有单个 CBV 描述符堆即可
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    cbvDesc.BufferLocation = ObjectCBuffer->Get()->GetGPUVirtualAddress();

    md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

}

void Box::BuildRootSigantureAndDescriptorTable() {

    // 以描述符表作为根参数
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        1,
        0
    );

    // 根参数
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    slotRootParameter[0].InitAsDescriptorTable(
        1,
        &cbvTable
    );

    // 根签名的描述
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        1, 
        slotRootParameter, 
        0, nullptr, 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf()
    );
    md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())
    );



}

void Box::BuildShadersAndInputLayout() {
    mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "vs_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void Box::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };
    psoDesc.PS = {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQualityLevels - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;

    md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf()));

}
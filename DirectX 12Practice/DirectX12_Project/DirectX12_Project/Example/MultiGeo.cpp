#include "MultiGeo.h"

MultiGeo::MultiGeo(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

MultiGeo::~MultiGeo()
{
}



bool MultiGeo::Initialize() {
    if (!D3DApp::Initialize())
        return false;
    
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildCBuffer();
    BuildRootSigantureAndDescriptorTable();
    BuildShadersAndInputLayout();

    BuildPSO();

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;
}
void MultiGeo::OnResize() {
    D3DApp::OnResize();

    DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}
void MultiGeo::Update() {

    // Convert Spherical to Cartesian coordinates.
    mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
    mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
    mEyePos.y = mRadius * cosf(mPhi);

    // Build the view matrix.
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&mView, view);

    mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % gNumFrameResource;
    mCurrentFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

    if (mCurrentFrameResourceIndex != 0 && mFence->GetCompletedValue() < mCurrentFrameResource->Fence) {
        HANDLE eventhandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        mFence->SetEventOnCompletion(mCurrentFrameResource->Fence,eventhandle);
        WaitForSingleObject(eventhandle, INFINITE);
        CloseHandle(eventhandle);
    }

    UpdateFrameResource();
}
void MultiGeo::Draw() {
    mCommandAllocator->Reset();
    
    mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get());

    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);
    
    

    CD3DX12_RESOURCE_BARRIER pbarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    mCommandList->ResourceBarrier(1, &pbarrier1);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    D3D12_CPU_DESCRIPTOR_HANDLE phandle1 = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE phandle2 = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &phandle1, true, &phandle2);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    Vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    Vbv.SizeInBytes = 8 * sizeof(Vertex);
    Vbv.StrideInBytes = sizeof(Vertex);
    Ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    Ibv.SizeInBytes = 36 * sizeof(std::uint16_t);
    Ibv.Format = DXGI_FORMAT_R16_UINT;

    mCommandList->IASetVertexBuffers(0, 1, &Vbv);
    mCommandList->IASetIndexBuffer(&Ibv);
    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    CD3DX12_RESOURCE_BARRIER&& pbarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &pbarrier2);

    mCommandList->DrawIndexedInstanced(
        36,
        1, 0, 0, 0);

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrentBuffer = (mCurrentBuffer + 1) % mSwapChainBufferCount;


    mCurrentFrameResource->Fence = ++mCurrentFence;

    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void MultiGeo::BuildShapeGeometry() {
    GeometryGenerator geoGen;

    GeometryGenerator::MeshData geoData[4] = {};
    geoData[0] = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
    geoData[1] = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    geoData[2] = geoGen.CreateSphere(0.5f, 20, 20);
    geoData[2] = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);


    // 合并 顶点/索引缓冲区
    UINT pVertexOffset[4] = {0};
    for (size_t i = 1; i < _countof(geoData); i++)
    {
        pVertexOffset[i] = pVertexOffset[i - 1] + (UINT)geoData[i].Vertices.size();
    }
    UINT pIndexOffset[4] = { 0 };
    for (size_t i = 1; i < _countof(geoData); i++)
    {
        pIndexOffset[i] = pIndexOffset[i - 1] + (UINT)geoData[i].Indices32.size();
    }

        // 定义 SubmeshGeometry 结构体中包含了顶点/索引缓冲区内不同几何体的子网格体数据
    SubmeshGeometry subMeshs[4];
    for (size_t i = 0; i < _countof(geoData); i++) {
        subMeshs[i].IndexCount = (UINT)geoData[i].Indices32.size();
        subMeshs[i].StartIndexLocation = pIndexOffset[i];
        subMeshs[i].BaseVertexLocation = pVertexOffset[i];
    }
        // 提取所有顶点元素，将顶点装入一个顶点缓冲区
    UINT totalVertexCount = 0;
    for (size_t i = 0; i < _countof(geoData); i++) {
        totalVertexCount += geoData[i].Vertices.size();
    }

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    DirectX::XMFLOAT4 colors[4] = {
        DirectX::XMFLOAT4(DirectX::Colors::ForestGreen),
        DirectX::XMFLOAT4(DirectX::Colors::ForestGreen),
        DirectX::XMFLOAT4(DirectX::Colors::ForestGreen),
        DirectX::XMFLOAT4(DirectX::Colors::ForestGreen)
    };
    for (size_t i = 0; i < _countof(geoData); ++i)
    {
        for (size_t j = 0; j < geoData[i].Vertices.size(); ++j,++k)
        {
            vertices[k].Pos = geoData->Vertices[j].Position;
            vertices[k].Color = colors[k];
        }
    }

    std::vector<std::uint16_t> indices;
    for (size_t i = 0; i < _countof(geoData); i++) {
        indices.insert(indices.end(), std::begin(geoData[i].GetIndices16()), std::end(geoData[i].GetIndices16()));
    }
    auto MeshGeo = std::make_unique<MeshGeometry>();
    MeshGeo->Name = "shapeGeo";


    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    D3DCreateBlob(vbByteSize, &MeshGeo->VertexBufferCPU);
    CopyMemory(MeshGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(),vbByteSize);

    D3DCreateBlob(ibByteSize, &MeshGeo->IndexBufferCPU);
    CopyMemory(MeshGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    MeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
        vertices.data(), vbByteSize,
        MeshGeo->VertexBufferUploader);
    MeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
        indices.data(), ibByteSize,
        MeshGeo->IndexBufferUploader);
    MeshGeo->VertexBufferByteSize = vbByteSize;
    MeshGeo->VertexByteStride = sizeof(Vertex);
    MeshGeo->IndexBufferByteSize = ibByteSize;
    MeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;

    
    MeshGeo->DrawArgs = std::move(std::vector<SubmeshGeometry>(geoData, geoData + _countof(geoData)));
    
    mGeometries[MeshGeo->Name] = std::move(MeshGeo);

}


void MultiGeo::BuildCBuffer() {
    ObjectCBuffer = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
    // 因为只要绘制一个物体，所以创建一个存有单个 CBV 描述符堆即可
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    cbvDesc.BufferLocation = ObjectCBuffer->Get()->GetGPUVirtualAddress();

    md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

}

void MultiGeo::BuildRootSigantureAndDescriptorTable() {

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

void MultiGeo::BuildShadersAndInputLayout() {
    mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void MultiGeo::BuildPSO() {
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

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf())));

}

void MultiGeo::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}


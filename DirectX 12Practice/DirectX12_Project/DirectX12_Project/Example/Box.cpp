#include "Box.h"

Box::Box(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

Box::~Box()
{
}



bool Box::Initialize() {
    if (!D3DApp::Initialize())
        return false;
    
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildCBuffer();
    BuildRootSigantureAndDescriptorTable();
    BuildShadersAndInputLayout();
    BuildVertexBuffers();
    BuildIndexBuffer();
    BuildPSO();

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;
}
void Box::OnResize() {
    D3DApp::OnResize();

    DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}
void Box::Update() {
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    // Build the view matrix.
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&mView, view);

    DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&mWorld);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProj);
    DirectX::XMMATRIX worldViewProj = world * view * proj;

    // Update the constant buffer with the latest worldViewProj matrix.
    BoxObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    ObjectCBuffer->CopyData(0, objConstants);
}
void Box::Draw() {
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
    Vbv.SizeInBytes = 8 * sizeof(BoxVertex);
    Vbv.StrideInBytes = sizeof(BoxVertex);
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

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void Box::BuildVertexBuffers() {
    BoxVertex vertexs[] = {
        { DirectX::XMFLOAT3(-1.0f,-1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::White) },
        { DirectX::XMFLOAT3(+1.0f,-1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::Black) },
        { DirectX::XMFLOAT3(+1.0f,+1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::Red) },
        { DirectX::XMFLOAT3(+1.0f,+1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Green) },
        { DirectX::XMFLOAT3(-1.0f,+1.0f,-1.0f),DirectX::XMFLOAT3(DirectX::Colors::Blue) },
        { DirectX::XMFLOAT3(-1.0f,+1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Yellow) },
        { DirectX::XMFLOAT3(-1.0f,-1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Cyan) },
        { DirectX::XMFLOAT3(+1.0f,-1.0f,+1.0f),DirectX::XMFLOAT3(DirectX::Colors::Magenta) },
    };

    const UINT64 vbByteSize = 8 * sizeof(BoxVertex);
    mVertexBuffer = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertexs, vbByteSize, mVertexUploadBuffer);

    //D3D12_VERTEX_BUFFER_VIEW Vbv = {};
    //Vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    //Vbv.SizeInBytes = vbByteSize;
    //Vbv.StrideInBytes = sizeof(Vertex);

    //mCommandList->IASetVertexBuffers(0,1,&Vbv);

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

    //D3D12_INDEX_BUFFER_VIEW Ibv = {};
    //Ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    //Ibv.SizeInBytes = ibByteSize;
    //Ibv.Format = DXGI_FORMAT_R16_UINT;
    //mCommandList->IASetIndexBuffer(&Ibv);


}

void Box::BuildCBuffer() {
    ObjectCBuffer = std::make_unique<UploadBuffer<BoxObjectConstants>>(md3dDevice.Get(), 1, true);
    // 因为只要绘制一个物体，所以创建一个存有单个 CBV 描述符堆即可
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(BoxObjectConstants));
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
    mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

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

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf())));

}

void Box::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}


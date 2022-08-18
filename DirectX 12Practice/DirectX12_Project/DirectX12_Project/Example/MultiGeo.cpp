#include "MultiGeo.h"
#include "DDSTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "DirectXHelpers.h"
//#include "../test/DDSTextureLoader.h"

const int gNumFrameResource = 3;

MultiGeo::MultiGeo(HINSTANCE hInstance)
    : D3DApp(hInstance)
{

}

MultiGeo::~MultiGeo()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}



bool MultiGeo::Initialize() {
    if (!D3DApp::Initialize())
        return false;
    
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

    LoadTexture();
    BuildRootSigantureAndDescriptorTable();
    BuildMaterials();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
    //BuildCBufferView();
    BuildShaderView();
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
    OnKeyboardInput();
    UpdateCamera();


    mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % gNumFrameResource;
    mCurrentFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

    if (mCurrentFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrentFrameResource->Fence) {
        HANDLE eventhandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        mFence->SetEventOnCompletion(mCurrentFrameResource->Fence,eventhandle);
        WaitForSingleObject(eventhandle, INFINITE);
        CloseHandle(eventhandle);
    }

    UpdateMaterials();
    UpdateObjectCBs();
    UpdateMainPassCB();

}
void MultiGeo::Draw() {

    auto cmdListAlloc = mCurrentFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    

    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO["opaque"].Get()));
    }

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

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvHeap.Get() }; //mCbvHeap.Get()
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    /*UINT passIndex = mCurrentFrameResourceIndex + (UINT)mOpaqueRitems.size() * gNumFrameResource;
    auto mHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    mHandle.Offset(passIndex, mCbvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(1, mHandle);*/
    auto passCB = mCurrentFrameResource->PassCB->Get();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());


    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);


    CD3DX12_RESOURCE_BARRIER&& pbarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &pbarrier2);


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

void MultiGeo::BuildMaterials() {
    auto mat = std::make_unique<Material>();
    auto mat1 = std::make_unique<Material>();
    auto mat2 = std::make_unique<Material>();
    auto mat3 = std::make_unique<Material>();
    auto mat4 = std::make_unique<Material>();
    
    mat->Name = "RedMat";
    mat->MatCBIndex = 0;
    mat->DiffuseSrvHeapIndex = 0;
    mat->DiffuseAlbedo = {1.0f,0.0f,0.0f,1.0f};
    mat->Routhness = 0.1f;
    

    mat1->Name = "BlueMat";
    mat1->MatCBIndex = 1;
    mat1->DiffuseSrvHeapIndex = 0;
    mat1->DiffuseAlbedo = { 0.0f,0.0f,1.0f,1.0f };
    mat1->Routhness = 0.1f;
    

    mat2->Name = "GreenMat";
    mat2->MatCBIndex = 2;
    mat2->DiffuseSrvHeapIndex = 0;
    mat2->DiffuseAlbedo = { 0.0f,1.0f,0.0f,1.0f };
    mat2->Routhness = 0.1f;
    

    mat3->Name = "GrayMat";
    mat3->MatCBIndex = 3;
    mat3->DiffuseSrvHeapIndex = 0;
    mat3->DiffuseAlbedo = { 0.5f,0.5f,0.5f,1.0f };
    mat3->Routhness = 0.5f;

    mat4->Name = "woodCrate";
    mat4->MatCBIndex = 4;
    mat4->DiffuseSrvHeapIndex = 0;
    mat4->DiffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
    mat4->Routhness = 0.2f;

    mMaterials["RedMat"] = std::move(mat);
    mMaterials["BlueMat"] = std::move(mat1);
    mMaterials["GreenMat"] = std::move(mat2);
    mMaterials["GrayMat"] = std::move(mat3);
    mMaterials["woodCrate"] = std::move(mat4);
}

void MultiGeo::BuildShapeGeometry() {

    GeometryGenerator geoGen;
    GeometryGenerator::MeshData geoData[4];
    geoData[0] = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    geoData[1] = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    geoData[2] = geoGen.CreateSphere(0.5f, 20, 20);
    geoData[3] = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    // 合并 顶点/索引缓冲区
    UINT pVertexOffset[4] = {0};
    for (size_t i = 1; i < _countof(geoData); i++)
    {
        pVertexOffset[i] = pVertexOffset[i - 1] + (UINT)geoData[i-1].Vertices.size();
    }
    UINT pIndexOffset[4] = { 0 };
    for (size_t i = 1; i < _countof(geoData); i++)
    {
        pIndexOffset[i] = pIndexOffset[i - 1] + (UINT)geoData[i-1].Indices32.size();
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

    for (size_t i = 0; i < _countof(geoData); i++)
    {
        for (size_t j = 0; j < geoData[i].Vertices.size(); j++,k++)
        {
            
            vertices[k].Pos = geoData[i].Vertices[j].Position;
            vertices[k].Normal = geoData[i].Vertices[j].Normal;
            vertices[k].TexC = geoData[i].Vertices[j].TexC;
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

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &MeshGeo->VertexBufferCPU));
    CopyMemory(MeshGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(),vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &MeshGeo->IndexBufferCPU));
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


    MeshGeo->DrawArgs = std::vector<SubmeshGeometry>(subMeshs, subMeshs + _countof(subMeshs));

    
    mGeometries[MeshGeo->Name] = std::move(MeshGeo);

}
void MultiGeo::BuildRenderItems(){

    
    auto boxRitem = std::make_unique<RenderItem>();
    //DirectX::XMStoreFloat4x4(&boxRitem->World,
    //    DirectX::XMMatrixTranslation(0.0f, 1.5f, 0.0f));

    boxRitem->ObjectCBIndex = 0;
    boxRitem->Geo = mGeometries["shapeGeo"].get();
    boxRitem->Mat = mMaterials["woodCrate"].get();
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs[0].IndexCount;
    boxRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs[0].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs[0].BaseVertexLocation;

    mAllRitems.push_back(std::move(boxRitem));

    //auto gridRitem = std::make_unique<RenderItem>();
    //gridRitem->World = MathHelper::Identity4x4();
    //gridRitem->ObjectCBIndex = 1;
    //gridRitem->Mat = mMaterials["GrayMat"].get();
    //gridRitem->Geo = mGeometries["shapeGeo"].get();
    //gridRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //gridRitem->IndexCount = gridRitem->Geo->DrawArgs[1].IndexCount;
    //gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs[1].StartIndexLocation;
    //gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs[1].BaseVertexLocation;
    //mAllRitems.push_back(std::move(gridRitem));

    //UINT objCBIndex = 2;
    //for (int i = 0; i < 5; ++i)
    //{
    //    auto leftCylRitem = std::make_unique<RenderItem>();
    //    auto rightCylRitem = std::make_unique<RenderItem>();
    //    auto leftSphereRitem = std::make_unique<RenderItem>();
    //    auto rightSphereRitem = std::make_unique<RenderItem>();

    //    DirectX::XMMATRIX leftCylWorld = DirectX::XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
    //    DirectX::XMMATRIX rightCylWorld = DirectX::XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

    //    DirectX::XMMATRIX leftSphereWorld = DirectX::XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
    //    DirectX::XMMATRIX rightSphereWorld = DirectX::XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

    //    XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
    //    leftCylRitem->ObjectCBIndex = objCBIndex++;
    //    leftCylRitem->Mat = mMaterials["BlueMat"].get();
    //    leftCylRitem->Geo = mGeometries["shapeGeo"].get();
    //    leftCylRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //    leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs[3].IndexCount;
    //    leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs[3].StartIndexLocation;
    //    leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs[3].BaseVertexLocation;

    //    XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
    //    rightCylRitem->ObjectCBIndex = objCBIndex++;
    //    rightCylRitem->Mat = mMaterials["BlueMat"].get();
    //    rightCylRitem->Geo = mGeometries["shapeGeo"].get();
    //    rightCylRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //    rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs[3].IndexCount;
    //    rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs[3].StartIndexLocation;
    //    rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs[3].BaseVertexLocation;

    //    XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
    //    leftSphereRitem->ObjectCBIndex = objCBIndex++;
    //    leftSphereRitem->Mat = mMaterials["GreenMat"].get();
    //    leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
    //    leftSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //    leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs[2].IndexCount;
    //    leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs[2].StartIndexLocation;
    //    leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs[2].BaseVertexLocation;

    //    XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
    //    rightSphereRitem->ObjectCBIndex = objCBIndex++;
    //    rightSphereRitem->Mat = mMaterials["GreenMat"].get();
    //    rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
    //    rightSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //    rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs[2].IndexCount;
    //    rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs[2].StartIndexLocation;
    //    rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs[2].BaseVertexLocation;

    //    mAllRitems.push_back(std::move(leftCylRitem));
    //    mAllRitems.push_back(std::move(rightCylRitem));
    //    mAllRitems.push_back(std::move(leftSphereRitem));
    //    mAllRitems.push_back(std::move(rightSphereRitem));
    //}

    for (auto& e : mAllRitems) {
        mOpaqueRitems.push_back(e.get());
    }

}
void MultiGeo::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResource; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}
void MultiGeo::BuildCBufferView() {
    UINT objByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT objCount = (UINT)mOpaqueRitems.size();

    for (int frameindex = 0; frameindex < gNumFrameResource; ++frameindex)
    {
        auto objectCB = mFrameResources[frameindex]->ObjectCB->Get();
        for (int i = 0; i < objCount; ++i)
        {
            D3D12_GPU_VIRTUAL_ADDRESS address = objectCB->GetGPUVirtualAddress();
            address += i * objByteSize;

            
            
            int heapIndex = frameindex * objCount + i;
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, mCbvUavDescriptorSize);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = address;
            cbvDesc.SizeInBytes = objByteSize;
            

            md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
    for (int frameindex = 0; frameindex < gNumFrameResource; ++frameindex)
    {
        auto PassCB = mFrameResources[frameindex]->PassCB->Get();

        D3D12_GPU_VIRTUAL_ADDRESS address = PassCB->GetGPUVirtualAddress();

        int heapIndex = (UINT)mOpaqueRitems.size() * gNumFrameResource + frameindex;
        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, mCbvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = address;
        cbvDesc.SizeInBytes = passByteSize;
        

        

        md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }



    

}

void MultiGeo::BuildShaderView() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mSrvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mTextures["crate"]->Resource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = mTextures["crate"]->Resource->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    
    //md3dDevice->CreateShaderResourceView(mTextures["crate"]->Resource.Get(), &srvDesc, handle);
    DirectX::CreateShaderResourceView(md3dDevice.Get(), mTextures["crate"]->Resource.Get(), handle);
    /*CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvHeap->GetCPUDescriptorHandleForHeapStart());

    auto woodCrateTex = mTextures["crate"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = woodCrateTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    md3dDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);*/
}

void MultiGeo::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {

    UINT objByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objCB = mCurrentFrameResource->ObjectCB->Get();
    auto matCB = mCurrentFrameResource->MatCB->Get();

    for (auto e : ritems)
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = e->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = e->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(e->primitiveType);

        /*UINT cbvIndex = mCurrentFrameResourceIndex * (UINT)mOpaqueRitems.size() + e->ObjectCBIndex;
        auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
        cbvHandle.Offset(cbvIndex, mCbvUavDescriptorSize);*/
        
        auto diffuseSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
        diffuseSrvHandle.Offset(e->Mat->DiffuseSrvHeapIndex, mCbvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objAddress = objCB->GetGPUVirtualAddress() + e->ObjectCBIndex * matByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matAddress = matCB->GetGPUVirtualAddress() + e->Mat->MatCBIndex * matByteSize;
        
        cmdList->SetGraphicsRootDescriptorTable(0, diffuseSrvHandle);
        /*cmdList->SetGraphicsRootDescriptorTable(2, cbvHandle);*/
        cmdList->SetGraphicsRootConstantBufferView(2, objAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matAddress);
        cmdList->DrawIndexedInstanced(e->IndexCount, 1, e->StartIndexLocation, e->BaseVertexLocation, 0);

    }

    
}

void MultiGeo::BuildRootSigantureAndDescriptorTable() {

    // 以描述符表作为根参数
    CD3DX12_DESCRIPTOR_RANGE cbvTable[3];
    cbvTable[0].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    /*cbvTable[1].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    cbvTable[2].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);*/
    // 根参数
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsDescriptorTable(
        1,
        &cbvTable[0],
        D3D12_SHADER_VISIBILITY_PIXEL
    );
    /*slotRootParameter[1].InitAsDescriptorTable(
        1,
        &cbvTable[1]
    );
    slotRootParameter[2].InitAsDescriptorTable(
        1,
        &cbvTable[2]
    );*/
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    // 根签名的描述
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        4, 
        slotRootParameter, 
        (UINT)staticSamplers.size(), staticSamplers.data(),
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
    mvsByteCode = d3dUtil::CompileShader(L"Shaders\\LightMultiGeo_TextureSampler.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"Shaders\\LightMultiGeo_TextureSampler.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void MultiGeo::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
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

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO["opaque"].GetAddressOf())));

    //
    // PSO for opaque wireframe objects.
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = psoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSO["opaque_wireframe"])));
}

void MultiGeo::BuildDescriptorHeaps()
{


    UINT objCount = (UINT)mOpaqueRitems.size();

    // 为每个帧资源的每个物体都创建一个CBV描述符
    // 为每帧的渲染过程增加一个CBV
    UINT numDesc = (objCount + 1) * gNumFrameResource;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = numDesc;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc,
        IID_PPV_ARGS(&mSrvHeap)));

}

void MultiGeo::UpdateCamera()
{
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
}

void MultiGeo::UpdateObjectCBs()
{
    auto currObjectCB = mCurrentFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        if (e->NumFramesDirty > 0)
        {
            DirectX::XMMATRIX world = XMLoadFloat4x4(&e->World);
            DirectX::XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

            currObjectCB->CopyData(e->ObjectCBIndex, objConstants);

            // Next FrameResource need to be updated too.
            e->NumFramesDirty--;
        }
    }
}

void MultiGeo::UpdateMainPassCB()
{
    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&mView);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProj);

    DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
    DirectX::XMVECTOR dmview = XMMatrixDeterminant(view);
    DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&dmview, view);
    DirectX::XMVECTOR dmproj = XMMatrixDeterminant(proj);
    DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&dmproj, proj);
    DirectX::XMVECTOR dmviewproj = XMMatrixDeterminant(viewProj);
    DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&dmviewproj, viewProj);

    DirectX::XMStoreFloat4x4(&mMainPassCB.View, DirectX::XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&mMainPassCB.InvView, DirectX::XMMatrixTranspose(invView));
    DirectX::XMStoreFloat4x4(&mMainPassCB.Proj, DirectX::XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&mMainPassCB.InvProj, DirectX::XMMatrixTranspose(invProj));
    DirectX::XMStoreFloat4x4(&mMainPassCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
    DirectX::XMStoreFloat4x4(&mMainPassCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = DirectX::XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = 0;
    mMainPassCB.DeltaTime = 0;
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = mCurrentFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}
void MultiGeo::UpdateMaterials() {
    auto MatetialCB = mCurrentFrameResource->MatCB.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {

            DirectX::XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialConstants matConstans;

            matConstans.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstans.FresnelR0 = mat->FresnelR0;
            matConstans.Routhness = mat->Routhness;
            DirectX::XMStoreFloat4x4(&matConstans.MatTransform, DirectX::XMMatrixTranspose(matTransform));
            MatetialCB->CopyData(mat->MatCBIndex, matConstans);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}
void MultiGeo::LoadTexture() {
    DirectX::ResourceUploadBatch resUploadBatch(md3dDevice.Get());
    auto tex = std::make_unique<Texture>();
    tex->Name = "crate";
    tex->Filename = L"Textures/WoodCrate01.dds";
    resUploadBatch.Begin();
    HRESULT hr = DirectX::CreateDDSTextureFromFile(md3dDevice.Get(), resUploadBatch, tex->Filename.c_str(), tex->Resource.GetAddressOf());

    ThrowIfFailed(hr);
    auto uploadResourcesFinished = resUploadBatch.End(mCommandQueue.Get());
    uploadResourcesFinished.wait();

    //mTextures["crate"] = std::move(tex);
    //auto woodCrateTex = std::make_unique<Texture>();
    //woodCrateTex->Name = "crate";
    //woodCrateTex->Filename = L"Textures/WoodCrate02.dds";
    //ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
    //    mCommandList.Get(), woodCrateTex->Filename.c_str(),
    //    woodCrateTex->Resource, woodCrateTex->UploadHeap));

    mTextures[tex->Name] = std::move(tex);
}
void MultiGeo::OnKeyboardInput()
{
    if (GetAsyncKeyState(VK_F4) & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
}
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> MultiGeo::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

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
        // ����
        0,4,2,
        0,2,1,

        // ����
        6,3,5,
        6,7,3,

        // ����
        6,5,4,
        6,4,0,

        // ����
        1,2,3,
        1,3,7,

        // ����
        4,5,3,
        4,3,2,

        // ����
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
    // ��ΪֻҪ����һ�����壬���Դ���һ�����е��� CBV �������Ѽ���
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    cbvDesc.BufferLocation = ObjectCBuffer->Get()->GetGPUVirtualAddress();

    md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

}

void Box::BuildRootSigantureAndDescriptorTable() {

    // ������������Ϊ������
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        1,
        0
    );

    // ������
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    slotRootParameter[0].InitAsDescriptorTable(
        1,
        &cbvTable
    );

    // ��ǩ��������
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
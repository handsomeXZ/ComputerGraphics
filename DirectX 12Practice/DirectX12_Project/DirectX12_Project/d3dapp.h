#pragma once

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

#include "d3dUtil.h"

class D3DApp
{
protected:
    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:
    static D3DApp* GetApp();

    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:

    bool InitMainWindow();
    bool InitDirect3D12();
    
    void CreateSwapChain();
    void FlushCommandQueue();

    virtual void OnResize();
    virtual void Update() = 0;
    virtual void Draw() = 0;
    
	ID3D12Resource* CurrentBackBuffer()const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView();

    void Modify4xMsaaState(bool prevstate);
    float AspectRatio()const;

protected:
    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle

    // ���ز���������The default is false.
    bool      m4xMsaaState = false;    // 4X MSAA enabled
    UINT m4xMsaaQualityLevels = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

    // Χ��
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT mCurrentFence = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    // ������
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static const INT mSwapChainBufferCount = 2;
    UINT mCurrentBuffer = 0;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    
    // ��������
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;
    UINT mRtvDescriptorSize = 0;
    UINT mDsvDescriptorSize = 0;
    UINT mCbvUavDescriptorSize = 0;

    // ������


    // ������
    Microsoft::WRL::ComPtr<ID3D12Resource> mBackBuffers[mSwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // �ӿڣ����þ���
    D3D12_VIEWPORT mViewport;
    D3D12_RECT mScissorRect;

    // Derived class should set these in derived constructor to customize starting values.
    std::wstring mMainWndCaption = L"d3d App";
    int mClientWidth = 800;
    int mClientHeight = 600;
    bool mAppPaused = false;
    bool mMinimized = false;
    bool mMaximized = true;
    bool mResizing = false;
};
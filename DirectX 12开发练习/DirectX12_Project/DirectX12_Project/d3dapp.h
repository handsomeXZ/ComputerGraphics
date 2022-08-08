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

    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:

    bool InitMainWindow();
    bool InitDirect3D12();
    bool CreateSwapChain();

protected:
    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle

    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;

    UINT mRtvDescriptorSize;
    UINT mDsvDescriptorSize;
    UINT mCbvUavDescriptorSize;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandList> mCommandList;

    // ½»»»Á´
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT SwapChainBufferCount = 2;
    UINT m4xMsaaQualityLevels;
    UINT mCurrentBuffer = 0;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView();
    

    // ÃèÊö·û¶Ñ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;


    // Derived class should set these in derived constructor to customize starting values.
    std::wstring mMainWndCaption = L"d3d App";
    int mClientWidth = 800;
    int mClientHeight = 600;
};


D3DApp::~D3DApp()
{
}
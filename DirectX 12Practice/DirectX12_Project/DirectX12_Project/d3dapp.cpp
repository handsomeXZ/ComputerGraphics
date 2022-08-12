#include "d3dapp.h"
#include "WindowsX.h"

using namespace std;


LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}
D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp()
{
	return mApp;
}
D3DApp::D3DApp(HINSTANCE hInstance)
    : mhAppInst(hInstance)
{
    // Only one D3DApp can be constructed.
    assert(mApp == nullptr);
    mApp = this;
}
D3DApp::~D3DApp() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

int D3DApp::Run() {
	MSG msg = { 0 };

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else 
		{
			if (!mAppPaused) {
				Update();
				Draw();

				
			}
			else
			{
				Sleep(100);
			}
		}
	}
	return (int)msg.wParam;
}
bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D12())
		return false;

	// Do the initial resize code.
	OnResize();

	return true;
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D12() {

	ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS (mdxgiFactory.GetAddressOf())));

	// 尝试创建硬件设备
	HRESULT hresultHardWare = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(md3dDevice.GetAddressOf()));
	
	// 回退到 Warp 设备
	if (FAILED(hresultHardWare)) {
		Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf())));// 只有在 IDXGIFactory4 里才有 EnumWarpAdapter

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(md3dDevice.GetAddressOf())
		));
	}

	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.GetAddressOf())));

	// 获取描述符大小
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	// 检测用户设备对 4X MSAA 质量级别的支持情况
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS mulsampleQualityLevels;
	mulsampleQualityLevels.Format = mBackBufferFormat;
	mulsampleQualityLevels.SampleCount = 4;
	mulsampleQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	mulsampleQualityLevels.NumQualityLevels = 0;

	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, // 多重采样的质量级别
		&mulsampleQualityLevels,
		sizeof(mulsampleQualityLevels)
	));
	m4xMsaaQualityLevels = mulsampleQualityLevels.NumQualityLevels;

	// 创建命令队列，命令列表，命令分配器
	D3D12_COMMAND_QUEUE_DESC queuedesc = {};
	queuedesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 可供GPU直接指向的命令
	queuedesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queuedesc,IID_PPV_ARGS(mCommandQueue.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.GetAddressOf())));
	ThrowIfFailed(md3dDevice->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));
	
	ThrowIfFailed(mCommandList->Close());

	CreateSwapChain();

	// 创建应用程序所需的描述符堆
		// 创建交换链所需的2个RenderTargetView
	D3D12_DESCRIPTOR_HEAP_DESC mRtvdesc = {};
	mRtvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	mRtvdesc.NumDescriptors = mSwapChainBufferCount;
	mRtvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	mRtvdesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&mRtvdesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));
		// 创建1个Depth/StencilView
	D3D12_DESCRIPTOR_HEAP_DESC mDsvdesc = {};
	mDsvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	mDsvdesc.NumDescriptors = 1;
	mDsvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	mDsvdesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&mDsvdesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));



	return true;
}

void D3DApp::CreateSwapChain() {
	// 创建交换链
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC swapChaindesc = {};
	swapChaindesc.BufferDesc.Width = mClientWidth;
	swapChaindesc.BufferDesc.Height = mClientHeight;
	swapChaindesc.BufferDesc.Format = mBackBufferFormat;
	swapChaindesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChaindesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChaindesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; // 扫描线顺序未指定
	swapChaindesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChaindesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	swapChaindesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQualityLevels - 1) : 0;
	swapChaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChaindesc.BufferCount = mSwapChainBufferCount;
	swapChaindesc.OutputWindow = mhMainWnd;
	swapChaindesc.Windowed = true;
	swapChaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChaindesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	mdxgiFactory->CreateSwapChain(mCommandQueue.Get(), &swapChaindesc, mSwapChain.GetAddressOf());

}
ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mBackBuffers[mCurrentBuffer].Get();
}
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() {
	//D3D12_CPU_DESCRIPTOR_HANDLE nextBackBuffer;
	//nextBackBuffer.ptr = mRtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + mCurrentBuffer * mRtvDescriptorSize;
	//return nextBackBuffer;
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrentBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() {
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::FlushCommandQueue() {
	assert(mFence);
	assert(mCommandQueue);

	mCurrentFence++;
	
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
	while (mFence->GetCompletedValue() < mCurrentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		mFence->SetEventOnCompletion(mCurrentFence, eventHandle);

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

}

void D3DApp::OnResize() {


	// 等待命令队列执行完毕
	FlushCommandQueue();

	mCommandList->Reset(mCommandAllocator.Get(), nullptr);

	// 释放缓冲区
	for (int i = 0; i < mSwapChainBufferCount; ++i) {
		mBackBuffers[i].Reset();
	}
	mDepthStencilBuffer.Reset();


	mSwapChain->ResizeBuffers(
		mSwapChainBufferCount,
		mClientWidth,
		mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	);
	mCurrentBuffer = 0;

	// 重新创建RenderTargetView
	CD3DX12_CPU_DESCRIPTOR_HANDLE mRtvViewFirstHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < mSwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffers[i].GetAddressOf())));

		md3dDevice->CreateRenderTargetView(mBackBuffers[i].Get(), nullptr, mRtvViewFirstHandle);

		mRtvViewFirstHandle.Offset(1, mRtvDescriptorSize);
	}



	// 重新创建 DepthStencilBuffer 和 DepthStencilView
	D3D12_HEAP_PROPERTIES pDSHeapProperties = {};
	pDSHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 深度模板缓冲区只需要GPU读写

	D3D12_RESOURCE_DESC pDSResouceDesc = {};
	pDSResouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	pDSResouceDesc.Width = mClientWidth;
	pDSResouceDesc.Height = mClientHeight;
	pDSResouceDesc.DepthOrArraySize = 1;
	pDSResouceDesc.MipLevels = 1;
	pDSResouceDesc.Format = mDepthStencilFormat;
	pDSResouceDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	pDSResouceDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQualityLevels - 1) : 0;
	pDSResouceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	pDSResouceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE opClear = {};
	opClear.Format = mDepthStencilFormat;
	opClear.DepthStencil.Depth = 1.0f;
	opClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&pDSHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&pDSResouceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&opClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())
	));

	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());

	// 将缓冲区资源从初始状转换为深度缓冲区

	CD3DX12_RESOURCE_BARRIER pbarrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ResourceBarrier(
		1,
		&pbarrier
	);

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	// 更新视口和剪裁矩形的配置信息
	
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<float>(mClientWidth);
	mViewport.Height = static_cast<float>(mClientHeight);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect = { 0 ,0 , mClientWidth, mClientHeight };

}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			//mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			//mTimer.Start();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		//mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		//mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		//OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		//OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		//OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Modify4xMsaaState(m4xMsaaState);

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void D3DApp::Modify4xMsaaState(bool prevstate) {
	m4xMsaaState = !prevstate;
}
float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}
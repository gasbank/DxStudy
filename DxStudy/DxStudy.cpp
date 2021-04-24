#include "framework.h"
#include "DxStudy.h"

#define MAX_LOAD_STRING 100

// Global Variables:
HINSTANCE hInst; // current instance
WCHAR szTitle[MAX_LOAD_STRING]; // The title bar text
WCHAR szWindowClass[MAX_LOAD_STRING]; // the main window class name

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

void InitDx();
void PopulateCommandList();
void OnRender();

using Microsoft::WRL::ComPtr;
using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr)
	{
	}

	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}

// *******************
static const UINT FrameCount = 2;
// *******************
HWND gHwnd;
ComPtr<ID3D12Device> gDevice;
ComPtr<ID3D12CommandQueue> gCommandQueue;
int gWidth = 640;
int gHeight = 480;
ComPtr<IDXGISwapChain3> gSwapChain;
UINT gFrameIndex;
ComPtr<ID3D12DescriptorHeap> gRtvHeap;
UINT gRtvDescriptorSize;
ComPtr<ID3D12Resource> gRenderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator> gCommandAllocator;
ComPtr<ID3D12RootSignature> gRootSignature;
ComPtr<ID3D12PipelineState> gPipelineState;
ComPtr<ID3D12GraphicsCommandList> gCommandList;
ComPtr<ID3D12Resource> gVertexBuffer;
D3D12_VERTEX_BUFFER_VIEW gVertexBufferView;
ComPtr<ID3D12Fence> gFence;
UINT64 gFenceValue;
HANDLE gFenceEvent;
CD3DX12_VIEWPORT gViewport;
CD3DX12_RECT gScissorRect;
// *******************

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOAD_STRING);
	LoadStringW(hInstance, IDC_DXSTUDY, szWindowClass, MAX_LOAD_STRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	MSG msg = {};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return static_cast<int>(msg.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DXSTUDY));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr; // MAKEINTRESOURCEW(IDC_DXSTUDY);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	RECT windowRect = {
		0,
		0,
		static_cast<LONG>(gWidth),
		static_cast<LONG>(gHeight)
	};
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	gHwnd = CreateWindowW(szWindowClass,
	                      szTitle,
	                      WS_OVERLAPPEDWINDOW,
	                      CW_USEDEFAULT,
	                      CW_USEDEFAULT,
	                      windowRect.right - windowRect.left,
	                      windowRect.bottom - windowRect.top,
	                      nullptr,
	                      nullptr,
	                      hInstance,
	                      nullptr);

	if (!gHwnd)
	{
		return FALSE;
	}

	InitDx();

	ShowWindow(gHwnd, nCmdShow);
	UpdateWindow(gHwnd);

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		{
			const int wmId = LOWORD(wParam);
			// Parse the menu selections:
			switch (wmId)
			{
			case IDM_ABOUT:
				DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
				break;
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
	case WM_PAINT:
		OnRender();
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return static_cast<INT_PTR>(TRUE);

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return static_cast<INT_PTR>(TRUE);
		}
		break;
	default:
		break;
	}
	return static_cast<INT_PTR>(FALSE);
}

void GetHardwareAdapter(
	IDXGIFactory1* pFactory,
	IDXGIAdapter1** ppAdapter,
	bool requestHighPerformanceAdapter = false)
{
	*ppAdapter = nullptr;

	ComPtr<IDXGIAdapter1> adapter;

	ComPtr<IDXGIFactory6> factory6;
	if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
	{
		for (
			UINT adapterIndex = 0;
			DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
				adapterIndex,
				requestHighPerformanceAdapter == true
					? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
					: DXGI_GPU_PREFERENCE_UNSPECIFIED,
				IID_PPV_ARGS(&adapter));
			++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}
	else
	{
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++
		     adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	*ppAdapter = adapter.Detach();
}

void WaitForPreviousFrame()
{
	const UINT64 fence = gFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), fence));
	gFenceValue++;

	const UINT64 completedValue = gFence->GetCompletedValue();

	if (completedValue < fence)
	{
		ThrowIfFailed(gFence->SetEventOnCompletion(fence, gFenceEvent));
		WaitForSingleObject(gFenceEvent, INFINITE);
	}

	gFrameIndex = gSwapChain->GetCurrentBackBufferIndex();
}

void InitDx()
{
#pragma region 디버그 레이어 활성화
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif
#pragma endregion

#pragma region IDXGIFactory4 factory 생성
	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
#pragma endregion

#pragma region IDXGIAdapter1 hardwareAdapter 생성
	ComPtr<IDXGIAdapter1> hardwareAdapter;
	GetHardwareAdapter(factory.Get(), &hardwareAdapter);
#pragma endregion

#pragma region ID3D12Device g_device 생성
	ThrowIfFailed(D3D12CreateDevice(
		hardwareAdapter.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&gDevice)
	));
#pragma endregion

#pragma region 커맨드큐 생성
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(gDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&gCommandQueue)));
#pragma endregion

#pragma region 스왑체인 생성
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = gWidth;
	swapChainDesc.Height = gHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		gCommandQueue.Get(),
		gHwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));
#pragma endregion

#pragma region HWND와 연결, 스왑체인 변수 변경, 프레임 인덱스 저장
	ThrowIfFailed(factory->MakeWindowAssociation(gHwnd, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain.As(&gSwapChain));
	gFrameIndex = gSwapChain->GetCurrentBackBufferIndex();
#pragma endregion

#pragma region Render Target View(RTV) 디스크립터 힙 생성
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(gDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&gRtvHeap)));

		gRtvDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
#pragma endregion

#pragma region Render Target View(RTV) 디스크립터 힙에서 디스크립터를 두 개 생성하기
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(gRtvHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(gSwapChain->GetBuffer(n, IID_PPV_ARGS(&gRenderTargets[n])));
			gDevice->CreateRenderTargetView(gRenderTargets[n].Get(),
			                                nullptr, rtvHandle);
			rtvHandle.Offset(1, gRtvDescriptorSize);
		}
	}
#pragma endregion

#pragma region 커맨드 할당자 생성
	ThrowIfFailed(gDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
	                                              IID_PPV_ARGS(&gCommandAllocator)));
#pragma endregion

#pragma region 루트 시그니쳐 생성
	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(0,
		                       nullptr,
		                       0,
		                       nullptr,
		                       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc,
		                                          D3D_ROOT_SIGNATURE_VERSION_1,
		                                          &signature,
		                                          &error));
		ThrowIfFailed(gDevice->CreateRootSignature(0,
		                                           signature->GetBufferPointer(),
		                                           signature->GetBufferSize(),
		                                           IID_PPV_ARGS(&gRootSignature)));
	}
#pragma endregion

#pragma region 셰이더 컴파일 및 Pipeline State Object (PSO) 생성
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0,
		                                 &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0,
		                                 &pixelShader, nullptr));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{"XXX_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"XXX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
		psoDesc.pRootSignature = gRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(gDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gPipelineState)));
	}
#pragma endregion

#pragma region 커맨드 리스트 생성
	ThrowIfFailed(gDevice->CreateCommandList(0,
	                                         D3D12_COMMAND_LIST_TYPE_DIRECT,
	                                         gCommandAllocator.Get(),
	                                         gPipelineState.Get(),
	                                         IID_PPV_ARGS(&gCommandList)));
	// 루프에서 열고 시작할 것이기 때문에 닫은 상태로 만든다.
	ThrowIfFailed(gCommandList->Close());
#pragma endregion

#pragma region gVertexBuffer 버텍스 버퍼 생성
	{
		Vertex triangleVertices[] =
		{
			{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
			{{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},

			{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
			{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		};

		const UINT vertexBufferSize = sizeof triangleVertices;

		const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

		ThrowIfFailed(gDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&gVertexBuffer)));

		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(gVertexBuffer->Map(0, &readRange,
		                                 reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		gVertexBuffer->Unmap(0, nullptr);

		gVertexBufferView.BufferLocation = gVertexBuffer->GetGPUVirtualAddress();
		gVertexBufferView.StrideInBytes = sizeof(Vertex);
		gVertexBufferView.SizeInBytes = vertexBufferSize;
	}
#pragma endregion

#pragma region Fence 생성
	ThrowIfFailed(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence)));
	gFenceValue = 1;

	gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (gFenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	WaitForPreviousFrame();
#pragma endregion

	gViewport = CD3DX12_VIEWPORT(0.0f,
	                             0.0f,
	                             static_cast<float>(gWidth),
	                             static_cast<float>(gHeight));
	gScissorRect = CD3DX12_RECT(0,
	                            0,
	                            static_cast<LONG>(gWidth),
	                            static_cast<LONG>(gHeight));
}

void PopulateCommandList()
{
	ThrowIfFailed(gCommandAllocator->Reset());
	ThrowIfFailed(gCommandList->Reset(gCommandAllocator.Get(),
	                                  gPipelineState.Get()));

	gCommandList->SetGraphicsRootSignature(gRootSignature.Get());
	gCommandList->RSSetViewports(1, &gViewport);
	gCommandList->RSSetScissorRects(1, &gScissorRect);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		gRenderTargets[gFrameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	gCommandList->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		gRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		gFrameIndex,
		gRtvDescriptorSize);

	gCommandList->OMSetRenderTargets(1,
	                                 &rtvHandle,
	                                 FALSE,
	                                 nullptr);

	const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
	gCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	gCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gCommandList->IASetVertexBuffers(0, 1, &gVertexBufferView);
	gCommandList->DrawInstanced(6,
	                            1,
	                            0,
	                            0);

	auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
		gRenderTargets[gFrameIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	gCommandList->ResourceBarrier(1, &barrier2);

	ThrowIfFailed(gCommandList->Close());
}

void OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = {gCommandList.Get()};
	gCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(gSwapChain->Present(1, 0));

	WaitForPreviousFrame();
}

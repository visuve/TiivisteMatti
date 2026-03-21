#include "PCH.hpp"

import HashLib;

namespace HashCalcGUI
{
	namespace IDs
	{
		enum Message : UINT
		{
			AddDiscoveredFiles = WM_APP + 1,
			ProcessPendingFiles,
			UpdateProgress,
			UpdateHash
		};

		enum Command : WORD
		{
			FileBrowse = 1001,
			FileExit = 1002,
			HelpAbout = 1003
		};

		enum Control : UINT_PTR
		{
			TreeView = 2001,
			StatusBar = 2002
		};
	};

	constexpr size_t BatchProcessSize = 1000;

	class MainWindow
	{
	public:
		struct DiscoveryTaskData
		{
			HWND Window;
			std::vector<std::filesystem::path> TargetPaths;
		};

		struct DiscoveryResult
		{
			std::vector<std::filesystem::path> Files;

			bool Empty() const
			{
				return Files.empty();
			}

			size_t Size() const
			{
				return Files.size();
			}

			void AddFile(const std::filesystem::path& path)
			{
				Files.push_back(path);
			}
		};

		struct HashTaskData
		{
			HWND Window;
			HTREEITEM FileNode;
			HTREEITEM ProgressNode;
			std::filesystem::path FilePath;
			std::vector<std::wstring> Algorithms;
			std::map<std::wstring, std::wstring> Results;
			std::stop_token StopToken;
		};

		bool Create(HINSTANCE instance, int cmdShow)
		{
			const DWORD maxThreads = std::min<DWORD>(3, std::thread::hardware_concurrency() - 1);

			_pool = CreateThreadpool(nullptr);
			SetThreadpoolThreadMaximum(_pool, maxThreads);
			InitializeThreadpoolEnvironment(&_threadPool);
			SetThreadpoolCallbackPool(&_threadPool, _pool);

			const wchar_t className[] = L"HashCalcWindow";
			WNDCLASSW windowClass = { 0 };
			windowClass.lpfnWndProc = StaticWndProc;
			windowClass.hInstance = instance;
			windowClass.lpszClassName = className;
			windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
			windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
			windowClass.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(COLOR_WINDOW + 1));

			RegisterClassW(&windowClass);

			_window = CreateWindowExW(
				0,
				className,
				L"Hash Calculator",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				800,
				600,
				nullptr,
				nullptr,
				instance,
				this);

			if (_window == nullptr)
			{
				return false;
			}

			ShowWindow(_window, cmdShow);
			UpdateWindow(_window);

			return true;
		}

		int Run()
		{
			MSG message = { 0 };

			while (GetMessageW(&message, nullptr, 0, 0))
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}

			return static_cast<int>(message.wParam);
		}

	private:
		HWND _window = nullptr;
		HWND _treeView = nullptr;
		HWND _statusBar = nullptr;
		int _totalFiles = 0;
		std::map<std::filesystem::path, HTREEITEM> _folderNodes;
		std::stop_source _stopSource;
		PTP_POOL _pool = nullptr;
		TP_CALLBACK_ENVIRON _threadPool;
		std::optional<int> _folderIconIndex;
		std::map<std::wstring, int> _iconCache;
		std::deque<std::filesystem::path> _pendingFiles;

		static LRESULT CALLBACK StaticWndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
		{
			MainWindow* self = nullptr;

			if (message == WM_NCCREATE)
			{
				CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lparam);
				self = static_cast<MainWindow*>(createStruct->lpCreateParams);
				SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
				self->_window = window;
			}
			else
			{
				self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
			}

			if (self != nullptr)
			{
				return self->WndProc(message, wparam, lparam);
			}

			return DefWindowProcW(window, message, wparam, lparam);
		}

		LRESULT WndProc(UINT message, WPARAM wparam, LPARAM lparam)
		{
			switch (message)
			{
			case WM_CREATE:
				OnCreate();
				break;
			case WM_SIZE:
				OnSize(lparam);
				break;
			case WM_COMMAND:
				OnCommand(LOWORD(wparam));
				break;
			case WM_DROPFILES:
				OnDropFiles(reinterpret_cast<HDROP>(wparam));
				break;
			case WM_DESTROY:
				_stopSource.request_stop();
				DestroyThreadpoolEnvironment(&_threadPool);
				CloseThreadpool(_pool);
				PostQuitMessage(0);
				break;
			case IDs::Message::AddDiscoveredFiles:
			{
				std::unique_ptr<DiscoveryResult> data(reinterpret_cast<DiscoveryResult*>(lparam));

				_pendingFiles.insert(
					_pendingFiles.end(),
					std::make_move_iterator(data->Files.begin()),
					std::make_move_iterator(data->Files.end())
				);

				PostMessageW(_window, IDs::Message::ProcessPendingFiles, 0, 0);
				break;
			}
			case IDs::Message::ProcessPendingFiles:
			{
				if (_pendingFiles.empty())
				{
					break;
				}

				SendMessageW(_treeView, WM_SETREDRAW, FALSE, 0);

				size_t processed = 0;

				while (!_pendingFiles.empty() && processed < BatchProcessSize)
				{
					AddFileToTree(_pendingFiles.front());
					_pendingFiles.pop_front();
					processed++;
				}

				SendMessageW(_treeView, WM_SETREDRAW, TRUE, 0);

				if (!_pendingFiles.empty())
				{
					PostMessageW(_window, IDs::Message::ProcessPendingFiles, 0, 0);
				}
				else
				{
					RedrawWindow(_treeView, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
					UpdateStatusBar();
				}
				break;
			}
			case IDs::Message::UpdateHash:
			{
				std::unique_ptr<HashTaskData> data(reinterpret_cast<HashTaskData*>(lparam));

				SendMessageW(_treeView, TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(data->ProgressNode));

				for (const std::wstring& algo : data->Algorithms)
				{
					TVINSERTSTRUCTW is = { 0 };
					is.hParent = data->FileNode;
					is.hInsertAfter = TVI_LAST;
					is.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
					is.item.iImage = I_IMAGENONE;
					is.item.iSelectedImage = I_IMAGENONE;
					is.item.pszText = data->Results[algo].data();

					SendMessageW(_treeView, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&is));
				}
				break;
			}
			case IDs::Message::UpdateProgress:
			{
				auto data = reinterpret_cast<HashTaskData*>(wparam);
				float percentage = std::bit_cast<float>(static_cast<uint32_t>(lparam));

				std::wstring text = std::format(L"Calculating... {:.2f}%", percentage);

				TVITEMW tvi = { 0 };
				tvi.mask = TVIF_TEXT;
				tvi.hItem = data->ProgressNode;
				tvi.pszText = text.data();

				SendMessageW(_treeView, TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));
				break;
			}
			default:
				return DefWindowProcW(_window, message, wparam, lparam);
			}
			return 0;
		}

		void OnCreate()
		{
			InitializeThreadpoolEnvironment(&_threadPool);

			_treeView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
				WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
				0, 0, 0, 0, _window, reinterpret_cast<HMENU>(IDs::TreeView), GetModuleHandle(nullptr), nullptr);

			SetWindowTheme(_treeView, L"Explorer", nullptr);

			SHFILEINFOW fileInfo = { 0 };
			HIMAGELIST imageList = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L"C:\\", 0, &fileInfo, sizeof(fileInfo), SHGFI_SYSICONINDEX | SHGFI_SMALLICON));
			SendMessageW(_treeView, TVM_SETIMAGELIST, TVSIL_NORMAL, reinterpret_cast<LPARAM>(imageList));

			_statusBar = CreateWindowExW(
				0,
				STATUSCLASSNAMEW,
				L"Ready. Drag & drop files or folders to initiate hash calculation!",
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				0,
				0,
				0,
				0,
				_window,
				reinterpret_cast<HMENU>(IDs::StatusBar),
				GetModuleHandle(nullptr),
				nullptr);

			HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
			SendMessageW(_treeView, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(FALSE, 0));
			SendMessageW(_statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(FALSE, 0));

			HMENU menu = CreateMenu();
			HMENU fileMenu = CreatePopupMenu();
			HMENU helpMenu = CreatePopupMenu();

			AppendMenuW(fileMenu, MF_STRING, IDs::FileBrowse, L"Browse");
			AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(fileMenu, MF_STRING, IDs::FileExit, L"Exit");

			AppendMenuW(helpMenu, MF_STRING, IDs::HelpAbout, L"About");

			AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
			AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"Help");
			SetMenu(_window, menu);

			DragAcceptFiles(_window, TRUE);
		}

		void OnSize(LPARAM lparam) const
		{
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);

			SendMessageW(_statusBar, WM_SIZE, 0, 0);
			RECT statusRect;
			GetWindowRect(_statusBar, &statusRect);

			int statusHeight = statusRect.bottom - statusRect.top;
			MoveWindow(_treeView, 0, 0, width, height - statusHeight, TRUE);
		}

		void OnCommand(WORD id)
		{
			switch (id)
			{
			case IDs::FileBrowse:
				HandleBrowse();
				break;
			case IDs::FileExit:
				SendMessageW(_window, WM_CLOSE, 0, 0);
				break;
			case IDs::HelpAbout:
				MessageBoxW(_window, L"Hash Calculator v1.0\n\nCopyright (c) visuve 2026", L"About", MB_OK | MB_ICONINFORMATION);
				break;
			}
		}

		void OnDropFiles(HDROP drop)
		{
			UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
			std::vector<std::filesystem::path> paths;
			paths.reserve(fileCount);

			for (UINT i = 0; i < fileCount; ++i)
			{
				UINT length = DragQueryFileW(drop, i, nullptr, 0) + 1;
				std::wstring buffer(length, L'\0');
				DragQueryFileW(drop, i, buffer.data(), length);
				buffer.resize(length - 1);
				paths.emplace_back(buffer);
			}

			DragFinish(drop);
			ProcessPathsAsync(paths);
		}

		void UpdateStatusBar()
		{
			std::wstring text = std::format(L"Files loaded: {}", _totalFiles);
			SendMessageW(_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
		}

		int GetSystemIconIndex(const std::filesystem::path& path, bool isFolder)
		{
			if (isFolder)
			{
				if (!_folderIconIndex.has_value())
				{
					SHFILEINFOW fileInfo = { 0 };

					SHGetFileInfoW(
						L"doesnotactuallyexist",
						FILE_ATTRIBUTE_DIRECTORY,
						&fileInfo,
						sizeof(fileInfo),
						SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

					_folderIconIndex = fileInfo.iIcon;
				}

				return _folderIconIndex.value();
			}
			else
			{
				std::wstring ext = path.extension().wstring();

				for (wchar_t& c : ext)
				{
					c = std::towlower(c);
				}

				const auto it = _iconCache.find(ext);

				if (it != _iconCache.cend())
				{
					return it->second;
				}

				SHFILEINFOW fileInfo = { 0 };
				std::wstring dummyName = L"doesnotactuallyexist" + ext;

				SHGetFileInfoW(
					dummyName.c_str(),
					FILE_ATTRIBUTE_NORMAL,
					&fileInfo,
					sizeof(fileInfo),
					SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

				_iconCache[ext] = fileInfo.iIcon;

				return fileInfo.iIcon;
			}
		}

		static VOID CALLBACK DiscoveryWorker(PTP_CALLBACK_INSTANCE, PVOID context)
		{
			std::unique_ptr<DiscoveryTaskData> task(static_cast<DiscoveryTaskData*>(context));
			std::unique_ptr<DiscoveryResult> result = std::make_unique<DiscoveryResult>();

			const auto dispatchBatch = [&]()
			{
				PostMessageW(task->Window, IDs::Message::AddDiscoveredFiles, 0, reinterpret_cast<LPARAM>(result.release()));
				result = std::make_unique<DiscoveryResult>();
			};

			for (const std::filesystem::path& target : task->TargetPaths)
			{
				if (std::filesystem::is_directory(target))
				{
					const auto options = std::filesystem::directory_options::skip_permission_denied;

					for (const auto& entry : std::filesystem::recursive_directory_iterator(target, options))
					{
						if (entry.is_regular_file())
						{
							result->AddFile(entry.path());

							if (result->Size() >= BatchProcessSize)
							{
								dispatchBatch();
							}
						}
					}
				}
				else if (std::filesystem::is_regular_file(target))
				{
					result->AddFile(target);

					if (result->Size() >= BatchProcessSize)
					{
						dispatchBatch();
					}
				}
			}

			if (!result->Empty())
			{
				dispatchBatch();
			}
		}

		static VOID CALLBACK HashWorker(PTP_CALLBACK_INSTANCE, PVOID context)
		{
			std::unique_ptr<HashTaskData> data(static_cast<HashTaskData*>(context));

			try
			{
				const auto progressCallback = [rawData = data.get()](float percentage)
				{
					uint32_t bits = std::bit_cast<uint32_t>(percentage);
					PostMessageW(rawData->Window, IDs::Message::UpdateProgress, reinterpret_cast<WPARAM>(rawData), static_cast<LPARAM>(bits));
				};

				HashLib::Calculator calc(data->Algorithms);
				std::map<std::wstring, std::wstring> hashes =
					calc.CalculateChecksumsFromFile(
						data->FilePath,
						data->StopToken,
						progressCallback);

				for (const auto& [algo, hash] : hashes)
				{
					data->Results[algo] = std::format(L"{}: {}", algo, hash);
				}
			}
			catch (const std::exception& e)
			{
				std::wstring error = HashLib::Strings::ToWide(e.what());

				for (const auto& algo : data->Algorithms)
				{
					data->Results[algo] = std::format(L"{}: Error: {}", algo, error);
				}
			}

			// Spin-wait until the queue has space
			while (!PostMessageW(data->Window, IDs::Message::UpdateHash, 0, reinterpret_cast<LPARAM>(data.get())))
			{
				Sleep(1);
			}

			data.release();
		}

		HTREEITEM InsertTreeItem(const TVINSERTSTRUCTW& is) const
		{
			LRESULT result = SendMessageW(_treeView, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&is));
			return reinterpret_cast<HTREEITEM>(result);
		}

		void AddFileToTree(const std::filesystem::path& filePath)
		{
			const std::filesystem::path folderPath = filePath.parent_path();
			HTREEITEM folderItem = nullptr;

			if (_folderNodes.find(folderPath) == _folderNodes.end())
			{
				int folderIcon = GetSystemIconIndex(folderPath, true);

				TVINSERTSTRUCTW insertStructFolder = { 0 };
				insertStructFolder.hParent = TVI_ROOT;
				insertStructFolder.hInsertAfter = TVI_LAST;
				insertStructFolder.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				insertStructFolder.item.pszText = const_cast<LPWSTR>(folderPath.c_str());
				insertStructFolder.item.iImage = folderIcon;
				insertStructFolder.item.iSelectedImage = folderIcon;

				folderItem = InsertTreeItem(insertStructFolder);
				_folderNodes[folderPath] = folderItem;
			}
			else
			{
				folderItem = _folderNodes[folderPath];
			}

			int fileIcon = GetSystemIconIndex(filePath, false);

			TVINSERTSTRUCTW insertStructFile = { 0 };
			insertStructFile.hParent = folderItem;
			insertStructFile.hInsertAfter = TVI_LAST;
			insertStructFile.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

			std::wstring fileName = filePath.filename().wstring();
			insertStructFile.item.pszText = const_cast<LPWSTR>(fileName.c_str());
			insertStructFile.item.iImage = fileIcon;
			insertStructFile.item.iSelectedImage = fileIcon;

			HTREEITEM fileItem = InsertTreeItem(insertStructFile);

			SendMessageW(_treeView, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(folderItem));

			if (fileItem != nullptr)
			{
				auto taskData = new HashTaskData{
					_window,
					fileItem,
					nullptr,
					filePath,
					{L"MD5", L"SHA1", L"SHA256"},
					{},
					_stopSource.get_token()
				};

				TVINSERTSTRUCTW insertStructHash = { 0 };
				insertStructHash.hParent = fileItem;
				insertStructHash.hInsertAfter = TVI_LAST;
				insertStructHash.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				insertStructHash.item.iImage = I_IMAGENONE;
				insertStructHash.item.iSelectedImage = I_IMAGENONE;

				std::wstring pendingText = L"Calculating... 0.00%";
				insertStructHash.item.pszText = pendingText.data();

				taskData->ProgressNode = InsertTreeItem(insertStructHash);

				if (!TrySubmitThreadpoolCallback(HashWorker, taskData, &_threadPool))
				{
					delete taskData;
				}

				SendMessageW(_treeView, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(fileItem));
			}

			_totalFiles++;
		}

		void ProcessPathsAsync(const std::vector<std::filesystem::path>& paths)
		{
			auto task = new DiscoveryTaskData{ _window, paths };

			if (!TrySubmitThreadpoolCallback(DiscoveryWorker, task, &_threadPool))
			{
				delete task;
			}
		}

		void HandleBrowse()
		{
			std::vector<wchar_t> buffer(0x100000, L'\0');

			OPENFILENAMEW ofn = { sizeof(ofn) };
			ofn.hwndOwner = _window;
			ofn.lpstrFile = buffer.data();
			ofn.nMaxFile = static_cast<DWORD>(buffer.size());
			ofn.lpstrFilter = L"All Files\0*.*\0";
			ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;

			if (GetOpenFileNameW(&ofn))
			{
				const wchar_t* ptr = ofn.lpstrFile;
				std::filesystem::path root(ptr);

				ptr += root.native().length() + 1;

				std::vector<std::filesystem::path> paths;

				if (*ptr == L'\0')
				{
					paths.emplace_back(root);
				}
				else
				{
					while (*ptr != L'\0')
					{
						std::wstring_view fileName(ptr);
						paths.emplace_back(root / fileName);
						ptr += fileName.length() + 1;
					}
				}

				ProcessPathsAsync(paths);
			}
			else if (CommDlgExtendedError() == FNERR_BUFFERTOOSMALL)
			{
				MessageBoxW(_window, L"Buffer limit exceeded. Select fewer files.", L"Error", MB_ICONERROR);
			}
		}
	};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int cmdShow)
{
	INITCOMMONCONTROLSEX initCtrl = { 0 };
	initCtrl.dwSize = sizeof(INITCOMMONCONTROLSEX);
	initCtrl.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES;

	if (!InitCommonControlsEx(&initCtrl))
	{
		return ERROR_CAN_NOT_COMPLETE;
	}

	HashCalcGUI::MainWindow app;

	if (!app.Create(instance, cmdShow))
	{
		return ERROR_INVALID_WINDOW_HANDLE;
	}

	return app.Run();
}
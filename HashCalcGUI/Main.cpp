#include "PCH.hpp"

import HashLib;

namespace HashCalcGUI
{
	namespace IDs
	{
		enum Message : UINT
		{
			UpdateHash = WM_APP + 1,
			UpdateProgress = WM_APP + 2
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

	class MainWindow
	{
	public:
		struct HashTaskData
		{
			HWND Window;
			std::filesystem::path FilePath;
			std::map<std::wstring, HTREEITEM> Items;
			std::map<std::wstring, std::wstring> Results;
			std::stop_token StopToken;
		};

		bool Create(HINSTANCE instance, int cmdShow)
		{
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
		TP_CALLBACK_ENVIRON _threadPool;

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
				PostQuitMessage(0);
				break;
			case IDs::Message::UpdateHash:
			{
				std::unique_ptr<HashTaskData> data(reinterpret_cast<HashTaskData*>(lparam));

				for (const auto& [algo, item] : data->Items)
				{
					TVITEMW tvi = { 0 };
					tvi.mask = TVIF_TEXT;
					tvi.hItem = item;
					tvi.pszText = data->Results[algo].data();

					SendMessageW(_treeView, TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));
				}
				break;
			}
			case IDs::Message::UpdateProgress:
			{
				HashTaskData* data = reinterpret_cast<HashTaskData*>(wparam);
				float percentage = std::bit_cast<float>(static_cast<uint32_t>(lparam));

				for (const auto& [algo, item] : data->Items)
				{
					std::wstring text = std::format(L"{}: {:.2f}%", algo, percentage);

					TVITEMW tvi = { 0 };
					tvi.mask = TVIF_TEXT;
					tvi.hItem = item;
					tvi.pszText = text.data();

					SendMessageW(_treeView, TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));
				}

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

			_statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				0, 0, 0, 0, _window, reinterpret_cast<HMENU>(IDs::StatusBar), GetModuleHandle(nullptr), nullptr);

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

			for (UINT i = 0; i < fileCount; ++i)
			{
				UINT length = DragQueryFileW(drop, i, nullptr, 0) + 1;
				std::wstring buffer(length, '\0');
				DragQueryFileW(drop, i, buffer.data(), length);

				ProcessPath(buffer.data());
			}

			DragFinish(drop);
		}

		void UpdateStatusBar()
		{
			std::wstring text = std::format(L"Files loaded: {}", _totalFiles);
			SendMessageW(_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
		}

		int GetSystemIconIndex(const std::filesystem::path& path, bool isFolder)
		{
			SHFILEINFOW fileInfo = { 0 };
			DWORD attributes = isFolder ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

			SHGetFileInfoW(path.c_str(), attributes, &fileInfo, sizeof(fileInfo),
				SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

			return fileInfo.iIcon;
		}

		static VOID CALLBACK HashWorker(PTP_CALLBACK_INSTANCE, PVOID context)
		{
			std::unique_ptr<HashTaskData> data(static_cast<HashTaskData*>(context));

			try
			{
				std::vector<std::wstring> algorithms;
				for (const auto& [algo, item] : data->Items)
				{
					algorithms.push_back(algo);
				}

				const auto progressCallback = [rawData = data.get()](float percentage)
				{
						uint32_t bits = std::bit_cast<uint32_t>(percentage);
					PostMessageW(rawData->Window, IDs::Message::UpdateProgress, reinterpret_cast<WPARAM>(rawData), static_cast<LPARAM>(bits));
				};

				HashLib::Calculator calc(algorithms);
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

				for (const auto& [algo, item] : data->Items)
				{
					data->Results[algo] = std::format(L"{}: Error: {}", algo, error);
				}
			}

			if (PostMessageW(data->Window, IDs::Message::UpdateHash, 0, reinterpret_cast<LPARAM>(data.get())))
			{
				data.release();
			}
		}

		HTREEITEM InsertTreeItem(const TVINSERTSTRUCTW& is) const
		{
			LRESULT result = SendMessageW(_treeView, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&is));
			return reinterpret_cast<HTREEITEM>(result);
		}

		void AddFileToTree(const std::filesystem::path& filePath)
		{
			if (!std::filesystem::is_regular_file(filePath))
			{
				return;
			}

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
				HashTaskData* taskData = new HashTaskData{ _window, filePath, {}, {}, _stopSource.get_token() };

				for (const std::wstring& algo : { L"MD5", L"SHA1", L"SHA256" })
				{
					TVINSERTSTRUCTW insertStructHash = { 0 };
					insertStructHash.hParent = fileItem;
					insertStructHash.hInsertAfter = TVI_LAST;
					insertStructHash.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
					insertStructHash.item.iImage = I_IMAGENONE;
					insertStructHash.item.iSelectedImage = I_IMAGENONE;

					std::wstring pendingText = algo + L": <Pending Calculation>";
					insertStructHash.item.pszText = pendingText.data();
					HTREEITEM hashItem = InsertTreeItem(insertStructHash);

					taskData->Items[algo] = hashItem;
				}

				if (!TrySubmitThreadpoolCallback(HashWorker, taskData, &_threadPool))
				{
					delete taskData;
				}

				SendMessageW(_treeView, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(fileItem));
			}

			_totalFiles++;
		}

		void ProcessPath(const std::filesystem::path& targetPath)
		{
			if (std::filesystem::is_directory(targetPath))
			{
				const auto rdi = std::filesystem::recursive_directory_iterator(targetPath, std::filesystem::directory_options::skip_permission_denied);

				for (const auto& entry : rdi)
				{
					if (entry.is_regular_file())
					{
						AddFileToTree(entry.path());
					}
				}
			}
			else
			{
				AddFileToTree(targetPath);
			}

			UpdateStatusBar();
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

				if (*ptr == L'\0')
				{
					ProcessPath(root);
				}
				else
				{
					while (*ptr != L'\0')
					{
						std::wstring_view fileName(ptr);
						ProcessPath(root / fileName);

						ptr += fileName.length() + 1;
					}
				}
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
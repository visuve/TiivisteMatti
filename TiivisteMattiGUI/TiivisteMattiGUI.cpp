#include "PCH.hpp"
#include "Resources.h"
#include "../Version.h"

import TiivisteMattiLib;

namespace TiivisteMatti
{
	namespace IDs
	{
		enum String : UINT
		{
			MenuFile = IDS_MenuFile,
			MenuFileBrowse = IDS_MenuFileBrowse,
			MenuFileExit = IDS_MenuFileExit,
			MenuHelp = IDS_MenuHelp,
			MenuHelpAbout = IDS_MenuHelpAbout,
			StatusReady = IDS_StatusReady,
			StatusProcessing = IDS_StatusProcessing,
			StatusFinished = IDS_StatusFinished,
			StatusError = IDS_StatusError,
			StatusProcessed = IDS_StatusProcessed,
			TreeCalculating = IDS_TreeCalculating,
			WindowTitle = IDS_WindowTitle,
			AboutTitle = IDS_AboutTitle,
			AboutText = IDS_AboutText,
			BrowseTitle = IDS_BrowseTitle,
			ErrorBufferLimit = IDS_ErrorBufferLimit,
			ErrorTitle = IDS_ErrorTitle,
			CtxMenuCopy = IDS_CtxMenuCopy,
			LastString
		};

		enum Message : UINT
		{
			UpdateProgress = WM_APP + 1,
			UpdateComplete,
			UpdateError,
			Finished
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

	struct ProgressData
	{
		std::filesystem::path FilePath;
		float Percentage;
	};

	struct ResultData
	{
		std::filesystem::path FilePath;
		std::map<std::wstring, std::wstring> Hashes;
		std::wstring ErrorMessage;
	};

	std::wstring LoadResourceString(UINT id)
	{
		wchar_t* stringPtr = nullptr;

		int length = LoadStringW(GetModuleHandleW(nullptr), id, reinterpret_cast<LPWSTR>(&stringPtr), 0);

		if (length == 0)
		{
			throw std::system_error(GetLastError(), std::system_category(),
				std::format("Failed to load string resource ID: {}", id));
		}

		return std::wstring(stringPtr, length);
	}

	std::map<UINT, std::wstring> UiStrings;

	void InitializeStrings()
	{
		for (UINT id = IDs::MenuFile; id < IDs::LastString; ++id)
		{
			UiStrings[id] = LoadResourceString(id);
		}
	}

	class MainWindow
	{
	public:
		void Create(HINSTANCE instance, int cmdShow)
		{
			const wchar_t className[] = L"TiivisteMattiWindow";
			WNDCLASSW windowClass = { 0 };
			windowClass.lpfnWndProc = WindowProcedure;
			windowClass.hInstance = instance;
			windowClass.lpszClassName = className;
			windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
			windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
			windowClass.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(COLOR_WINDOW + 1));

			RegisterClassW(&windowClass);

			_window = CreateWindowExW(
				0,
				className,
				UiStrings.at(IDs::WindowTitle).c_str(),
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
				throw std::system_error(GetLastError(), std::system_category(), "CreateWindowExW failed");
			}

			ShowWindow(_window, cmdShow);
			UpdateWindow(_window);
		}

		int Run()
		{
			if (__argc > 1)
			{
				std::vector<std::filesystem::path> paths;

				for (size_t i = 1; i < __argc; ++i)
				{
					paths.emplace_back(__wargv[i]);
				}

				if (!paths.empty())
				{
					ProcessPathsAsync(paths);
				}
			}

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
		int _completedFiles = 0;
		int _errorFiles = 0;
		int _dotCount = -1;
		std::chrono::system_clock::time_point _startTime;

		std::optional<int> _folderIconIndex;
		std::map<std::wstring, int> _iconCache;

		std::map<std::filesystem::path, HTREEITEM> _folderNodes;
		std::map<std::filesystem::path, HTREEITEM> _fileNodes;
		std::map<std::filesystem::path, HTREEITEM> _progressNodes;

		TiivisteMattiLib::Calculator _calculator{ {L"MD5", L"SHA1", L"SHA256"} };
		std::vector<std::jthread> _workerThreads;
		std::atomic<int> _activeThreads;
		std::mutex _mutex;

		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
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
				return self->HandleMessage(message, wparam, lparam);
			}

			return DefWindowProcW(window, message, wparam, lparam);
		}

		LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam)
		{
			try
			{
				switch (message)
				{
				case WM_CREATE:
					OnCreate();
					break;
				case WM_SIZE:
					OnSize(lparam);
					break;
				case WM_NOTIFY:
					return OnNotify(wparam, lparam);
					break;
				case WM_CONTEXTMENU:
				{
					if (reinterpret_cast<HWND>(wparam) == _treeView)
					{
						OnTreeViewContextMenu(lparam);
					}
					break;
				}
				case WM_COMMAND:
					OnCommand(LOWORD(wparam));
					break;
				case WM_DROPFILES:
					OnDropFiles(reinterpret_cast<HDROP>(wparam));
					break;
				case WM_DESTROY:
				{
					for (auto& thread : _workerThreads)
					{
						if (thread.joinable())
						{
							thread.request_stop();
						}
					}

					PostQuitMessage(0);
					break;
				}
				case IDs::Message::UpdateProgress:
				{
					std::unique_ptr<ProgressData> data(reinterpret_cast<ProgressData*>(lparam));

					if (_progressNodes.find(data->FilePath) == _progressNodes.end())
					{
						AddFileToTree(data->FilePath);
						UpdateStatusBar(message);
					}

					std::wstring text = std::vformat(UiStrings.at(IDs::TreeCalculating), std::make_wformat_args(data->Percentage));

					TVITEMW tvi = { 0 };
					tvi.mask = TVIF_TEXT;
					tvi.hItem = _progressNodes[data->FilePath];
					tvi.pszText = text.data();

					SendMessageW(_treeView, TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));
					break;
				}
				case IDs::Message::UpdateComplete:
				{
					std::unique_ptr<ResultData> data(reinterpret_cast<ResultData*>(lparam));

					if (_fileNodes.find(data->FilePath) == _fileNodes.end())
					{
						AddFileToTree(data->FilePath); // Failsafe for 0-byte files
					}

					FinalizeFileNode(data->FilePath, data->Hashes, L"");
					++_completedFiles;
					UpdateStatusBar(message);
					break;
				}
				case IDs::Message::UpdateError:
				{
					std::unique_ptr<ResultData> data(reinterpret_cast<ResultData*>(lparam));

					if (_fileNodes.find(data->FilePath) == _fileNodes.end())
					{
						AddFileToTree(data->FilePath);
					}

					FinalizeFileNode(data->FilePath, {}, data->ErrorMessage);
					++_errorFiles;
					UpdateStatusBar(message);
					break;
				}
				case IDs::Message::Finished:
				{
					_ASSERT(_activeThreads == 0);
					_workerThreads.clear();
					UpdateStatusBar(message);
					break;
				}
				default:
					return DefWindowProcW(_window, message, wparam, lparam);
				}
			}
			catch (const std::exception& e)
			{
				std::wstring errorMsg = TiivisteMattiLib::Strings::ToWide(e.what());
				MessageBoxW(_window, errorMsg.c_str(), UiStrings.at(IDs::ErrorTitle).c_str(), MB_OK | MB_ICONERROR);
			}

			return 0;
		}

		void CreateMainMenu()
		{
			HMENU menu = CreateMenu();
			HMENU fileMenu = CreatePopupMenu();
			HMENU helpMenu = CreatePopupMenu();

			AppendMenuW(fileMenu, MF_STRING, IDs::FileBrowse, UiStrings.at(IDs::MenuFileBrowse).c_str());
			AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(fileMenu, MF_STRING, IDs::FileExit, UiStrings.at(IDs::MenuFileExit).c_str());

			AppendMenuW(helpMenu, MF_STRING, IDs::HelpAbout, UiStrings.at(IDs::MenuHelpAbout).c_str());

			AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), UiStrings.at(IDs::MenuFile).c_str());
			AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), UiStrings.at(IDs::MenuHelp).c_str());

			SetMenu(_window, menu);
		}

		void CreateTreeView()
		{
			_treeView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
				WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
				0, 0, 0, 0, _window, reinterpret_cast<HMENU>(IDs::TreeView), GetModuleHandle(nullptr), nullptr);

			SetWindowTheme(_treeView, L"Explorer", nullptr);

			SHFILEINFOW fileInfo = { 0 };
			HIMAGELIST imageList = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L"C:\\", 0, &fileInfo, sizeof(fileInfo), SHGFI_SYSICONINDEX | SHGFI_SMALLICON));
			SendMessageW(_treeView, TVM_SETIMAGELIST, TVSIL_NORMAL, reinterpret_cast<LPARAM>(imageList));

			HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
			SendMessageW(_treeView, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(FALSE, 0));
		}

		void OnCreate()
		{
			CreateTreeView();
			CreateMainMenu();

			_statusBar = CreateWindowExW(
				0,
				STATUSCLASSNAMEW,
				UiStrings.at(IDs::StatusReady).c_str(), // Localized initial state
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				0, 0, 0, 0, _window,
				reinterpret_cast<HMENU>(IDs::StatusBar),
				GetModuleHandle(nullptr), nullptr);

			HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
			SendMessageW(_statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(FALSE, 0));

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
				{
					const std::wstring wideVersion = TiivisteMattiLib::Strings::ToWide(TIIVISTEMATTI_VERSION);
					const std::wstring wideHash = TiivisteMattiLib::Strings::ToWide(TIIVISTEMATTI_COMMIT_HASH);
					const std::wstring text = std::vformat(UiStrings.at(IDs::AboutText), std::make_wformat_args(wideVersion, wideHash));
					MessageBoxW(_window, text.c_str(), UiStrings.at(IDs::AboutTitle).c_str(), MB_OK | MB_ICONINFORMATION);
					break;
				}
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

		void UpdateStatusBar(UINT message)
		{
			std::wstring status;

			switch (message)
			{
				case IDs::Message::UpdateProgress:
				case IDs::Message::UpdateComplete:
				{
					status = UiStrings.at(IDs::StatusProcessing);
					int dots = 50 - std::abs(50 - (++_dotCount % 100));
					status.append(dots, L'.');
					break;
				}
				case IDs::Message::UpdateError:
				{
					status = UiStrings.at(IDs::StatusError);
					break;
				}
				case IDs::Message::Finished:
				{
					const auto now = std::chrono::system_clock::now();
					const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime);
					status = std::format(L"{} {}", UiStrings.at(IDs::StatusFinished), std::chrono::hh_mm_ss(elapsed));
					break;
				}
			}

			std::wstring text = std::vformat(
				UiStrings.at(IDs::StatusProcessed),
				std::make_wformat_args(_completedFiles, _errorFiles, status));

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

				std::ranges::transform(ext, ext.begin(), [](wchar_t c)
				{
					return std::towlower(c);
				});

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

		HTREEITEM InsertTreeItem(const TVINSERTSTRUCTW& is) const
		{
			LRESULT result = SendMessageW(_treeView, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&is));
			return reinterpret_cast<HTREEITEM>(result);
		}

		void AddFileToTree(const std::filesystem::path& filePath)
		{
			const std::filesystem::path folderPath = filePath.parent_path();
			HTREEITEM folderItem = nullptr;
			bool isNewFolder = false;

			const auto it = _folderNodes.find(folderPath);

			if (it == _folderNodes.end())
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
				isNewFolder = true;
			}
			else
			{
				folderItem = it->second;
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
			_fileNodes[filePath] = fileItem;

			if (isNewFolder)
			{
				SendMessageW(_treeView, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(folderItem));
			}

			if (fileItem != nullptr)
			{
				TVINSERTSTRUCTW insertStructHash = { 0 };
				insertStructHash.hParent = fileItem;
				insertStructHash.hInsertAfter = TVI_LAST;
				insertStructHash.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				insertStructHash.item.iImage = I_IMAGENONE;
				insertStructHash.item.iSelectedImage = I_IMAGENONE;

				float initialPercentage = 0.0f;
				std::wstring pendingText = std::vformat(UiStrings.at(IDs::TreeCalculating), std::make_wformat_args(initialPercentage));
				insertStructHash.item.pszText = pendingText.data();

				_progressNodes[filePath] = InsertTreeItem(insertStructHash);

				SendMessageW(_treeView, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(fileItem));
			}
		}

		void FinalizeFileNode(const std::filesystem::path& filePath, const std::map<std::wstring, std::wstring>& hashes, const std::wstring& error)
		{
			auto progIt = _progressNodes.find(filePath);
			auto fileIt = _fileNodes.find(filePath);

			if (progIt != _progressNodes.end() && fileIt != _fileNodes.end())
			{
				SendMessageW(_treeView, TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(progIt->second));
				_progressNodes.erase(progIt);

				if (!error.empty())
				{
					TVINSERTSTRUCTW is = { 0 };
					is.hParent = fileIt->second;
					is.hInsertAfter = TVI_LAST;
					is.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
					is.item.iImage = I_IMAGENONE;
					is.item.iSelectedImage = I_IMAGENONE;

					std::wstring errText = std::format(L"{}: {}", UiStrings.at(IDs::StatusError), error);
					is.item.pszText = errText.data();

					InsertTreeItem(is);
				}
				else
				{
					int index = 0;

					for (const auto& [algo, hash] : hashes)
					{
						TVINSERTSTRUCTW is = { 0 };
						is.hParent = fileIt->second;
						is.hInsertAfter = TVI_LAST;
						is.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
						is.item.iImage = I_IMAGENONE;
						is.item.iSelectedImage = I_IMAGENONE;
						is.item.lParam = ++index;

						std::wstring resultText = std::format(L"{}: {}", algo, hash);
						is.item.pszText = resultText.data();

						InsertTreeItem(is);
					}
				}
			}
		}

		void ProcessPathsAsync(const std::vector<std::filesystem::path>& paths)
		{
			if (_activeThreads++ == 0)
			{
				_startTime = std::chrono::system_clock::now();
				_completedFiles = 0;
				_errorFiles = 0;
				_dotCount = -1;
			}

			TiivisteMattiLib::AsyncCallbacks callbacks;

			callbacks.OnStart = [this]()
			{
				_mutex.lock();
			};

			callbacks.OnProgress = [hwnd = _window](const std::filesystem::path& path, float percentage)
			{
				auto data = new ProgressData{ path, percentage };

				if (!PostMessageW(hwnd, IDs::Message::UpdateProgress, 0, reinterpret_cast<LPARAM>(data)))
				{
					delete data;
				}
			};

			callbacks.OnComplete = [hwnd = _window](const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& hashes)
			{
				auto data = new ResultData{ path, hashes, L"" };

				if (!PostMessageW(hwnd, IDs::Message::UpdateComplete, 0, reinterpret_cast<LPARAM>(data)))
				{
					delete data;
				}
			};

			callbacks.OnError = [hwnd = _window](const std::filesystem::path& path, const std::wstring& errorMsg)
			{
				auto data = new ResultData{ path, {}, errorMsg };

				if (!PostMessageW(hwnd, IDs::Message::UpdateError, 0, reinterpret_cast<LPARAM>(data)))
				{
					delete data;
				}
			};

			callbacks.OnFinished = [this]()
			{
				_mutex.unlock();

				if (--_activeThreads == 0)
				{
					PostMessageW(_window, IDs::Message::Finished, 0, 0);
				}
			};

			_workerThreads.emplace_back(_calculator.CalculateChecksumsAsync(paths, std::move(callbacks)));
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
				MessageBoxW(_window, UiStrings.at(IDs::ErrorBufferLimit).c_str(), UiStrings.at(IDs::ErrorTitle).c_str(), MB_ICONERROR);
			}
		}

		LRESULT OnNotify(WPARAM wparam, LPARAM lparam)
		{
			LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lparam);

			if (nmhdr->idFrom == IDs::TreeView && nmhdr->code == NM_CUSTOMDRAW)
			{
				LPNMTVCUSTOMDRAW tvcd = reinterpret_cast<LPNMTVCUSTOMDRAW>(lparam);

				switch (tvcd->nmcd.dwDrawStage)
				{
				case CDDS_PREPAINT:
				{
					return CDRF_NOTIFYITEMDRAW;
				}
				case CDDS_ITEMPREPAINT:
				{
					LPARAM index = tvcd->nmcd.lItemlParam;

					switch (index)
					{
						case 1: // MD5
						{
							tvcd->clrText = RGB(150, 0, 0);
							return CDRF_NEWFONT;
						}
						case 2: // SHA1
						{
							tvcd->clrText = RGB(180, 90, 0); 
							return CDRF_NEWFONT;
						}
						case 3: // SHA256
						{
							tvcd->clrText = RGB(0, 100, 0);
							return CDRF_NEWFONT;
						}
					}

					return CDRF_DODEFAULT;
				}
				default:
				{
					return CDRF_DODEFAULT;
				}
				}
			}

			return DefWindowProcW(_window, WM_NOTIFY, wparam, lparam);
		}

		void OnTreeViewContextMenu(LPARAM lparam)
		{
			POINT pt = { static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)) };

			POINT clientPt = pt;
			ScreenToClient(_treeView, &clientPt);

			TVHITTESTINFO ht = { 0 };
			ht.pt = clientPt;
			HTREEITEM item = reinterpret_cast<HTREEITEM>(SendMessageW(_treeView, TVM_HITTEST, 0, reinterpret_cast<LPARAM>(&ht)));

			if (!item)
			{
				return;
			}

			TVITEMW tvi = { 0 };
			tvi.mask = TVIF_PARAM;
			tvi.hItem = item;
			SendMessageW(_treeView, TVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));

			// Check if the clicked item is a hash result (lParam is 1, 2, or 3)
			if (tvi.lParam < 1 || tvi.lParam > 3)
			{
				return;
			}

			// Explicitly select the right-clicked item to prevent focus jumping
			SendMessageW(_treeView, TVM_SELECTITEM, TVGN_CARET, reinterpret_cast<LPARAM>(item));

			HMENU ctxMenu = CreatePopupMenu();
			AppendMenuW(ctxMenu, MF_STRING, 1, UiStrings.at(IDs::CtxMenuCopy).c_str());

			int cmd = TrackPopupMenu(ctxMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, _window, nullptr);
			DestroyMenu(ctxMenu);

			if (cmd == 1)
			{
				CopyHashToClipboard(item);
			}
		}

		void CopyHashToClipboard(HTREEITEM hItem)
		{
			constexpr int BufferSize = 0x400;
			wchar_t buffer[BufferSize] = { 0 };
			TVITEMW tvi = { 0 };
			tvi.mask = TVIF_TEXT;
			tvi.hItem = hItem;
			tvi.pszText = buffer;
			tvi.cchTextMax = BufferSize;
			SendMessageW(_treeView, TVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));

			std::wstring text(buffer);

			if (!OpenClipboard(_window))
			{
				return;
			}

			EmptyClipboard();
			size_t byteSize = (text.length() + 1) * sizeof(wchar_t);
			HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byteSize);

			if (memory != nullptr)
			{
				void* ptr = GlobalLock(memory);
				std::memcpy(ptr, text.c_str(), byteSize);
				GlobalUnlock(memory);
				SetClipboardData(CF_UNICODETEXT, memory);
			}

			CloseClipboard();
		}
	};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int cmdShow)
{
	// Uncomment to force locale to something:
	// SetThreadUILanguage(MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT));
	// SetThreadUILanguage(MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT));

	try
	{
		TiivisteMatti::InitializeStrings();

		INITCOMMONCONTROLSEX initCtrl = { 0 };
		initCtrl.dwSize = sizeof(INITCOMMONCONTROLSEX);
		initCtrl.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES;

		if (!InitCommonControlsEx(&initCtrl))
		{
			throw std::system_error(GetLastError(), std::system_category(), "InitCommonControlsEx failed");
		}

		TiivisteMatti::MainWindow app;
		app.Create(instance, cmdShow);

		return app.Run();
	}
	catch (const std::system_error& e)
	{
		std::string msg = std::format("Fatal System Error:\n{}\nCode: {}", e.what(), e.code().value());
		MessageBoxA(nullptr, msg.c_str(), "Initialization Failure", MB_OK | MB_ICONERROR);
		return e.code().value();
	}
	catch (const std::exception& e)
	{
		std::string msg = std::format("Fatal Error:\n{}", e.what());
		MessageBoxA(nullptr, msg.c_str(), "Initialization Failure", MB_OK | MB_ICONERROR);
		return ERROR_UNHANDLED_EXCEPTION;
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"An unknown catastrophic error occurred.", L"Initialization Failure", MB_OK | MB_ICONERROR);
		return ERROR_UNHANDLED_EXCEPTION;
	}
}
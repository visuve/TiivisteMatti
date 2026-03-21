#include "PCH.hpp"

import HashLib;

namespace HashCalcGUI
{
	namespace IDs
	{
		enum Message : UINT
		{
			UpdateProgress = WM_APP + 1,
			UpdateComplete,
			UpdateError,
			AllFinished
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

	class MainWindow
	{
	public:
		void Create(HINSTANCE instance, int cmdShow)
		{
			const wchar_t className[] = L"HashCalcWindow";
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

		std::optional<int> _folderIconIndex;
		std::map<std::wstring, int> _iconCache;

		std::map<std::filesystem::path, HTREEITEM> _folderNodes;
		std::map<std::filesystem::path, HTREEITEM> _fileNodes;
		std::map<std::filesystem::path, HTREEITEM> _progressNodes;

		HashLib::Calculator _calculator{ {L"MD5", L"SHA1", L"SHA256"} };
		std::jthread _workerThread;

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
				case WM_COMMAND:
					OnCommand(LOWORD(wparam));
					break;
				case WM_DROPFILES:
					OnDropFiles(reinterpret_cast<HDROP>(wparam));
					break;
				case WM_DESTROY:
					if (_workerThread.joinable())
					{
						_workerThread.request_stop();
					}
					PostQuitMessage(0);
					break;

				case IDs::Message::UpdateProgress:
				{
					std::unique_ptr<ProgressData> data(reinterpret_cast<ProgressData*>(lparam));

					if (_progressNodes.find(data->FilePath) == _progressNodes.end())
					{
						AddFileToTree(data->FilePath);
						UpdateStatusBar(L"Processing");
					}

					std::wstring text = std::format(L"Calculating... {:.2f}%", data->Percentage);

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
					UpdateStatusBar(L"Processing");
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
					UpdateStatusBar(L"Error");
					break;
				}
				case IDs::Message::AllFinished:
				{
					UpdateStatusBar(L"Finished");
					break;
				}
				default:
					return DefWindowProcW(_window, message, wparam, lparam);
				}
			}
			catch (const std::exception& e)
			{
				std::wstring errorMsg = HashLib::Strings::ToWide(e.what());
				MessageBoxW(_window, errorMsg.c_str(), L"Application Error", MB_OK | MB_ICONERROR);
			}

			return 0;
		}

		void OnCreate()
		{
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

		void UpdateStatusBar(std::wstring_view status = L"")
		{
			std::wstring displayStatus(status);

			if (status == L"Processing")
			{
				int dots = 50 - std::abs(50 - (++_dotCount % 100));
				displayStatus.append(dots, L'.');
			}

			std::wstring text = std::format(
				L"Processed: {} | Errors: {} | {}",
				_completedFiles,
				_errorFiles,
				displayStatus);

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

				std::wstring pendingText = L"Calculating... 0.00%";
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

					std::wstring errText = L"Error: " + error;
					is.item.pszText = errText.data();

					InsertTreeItem(is);
				}
				else
				{
					for (const auto& [algo, hash] : hashes)
					{
						TVINSERTSTRUCTW is = { 0 };
						is.hParent = fileIt->second;
						is.hInsertAfter = TVI_LAST;
						is.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
						is.item.iImage = I_IMAGENONE;
						is.item.iSelectedImage = I_IMAGENONE;

						std::wstring resultText = std::format(L"{}: {}", algo, hash);
						is.item.pszText = resultText.data();

						InsertTreeItem(is);
					}
				}
			}
		}

		void ProcessPathsAsync(const std::vector<std::filesystem::path>& paths)
		{
			if (_workerThread.joinable())
			{
				_workerThread.request_stop();
				_workerThread.join();
			}

			HashLib::AsyncCallbacks callbacks;

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

			callbacks.OnFinished = [hwnd = _window]()
			{
				PostMessageW(hwnd, IDs::Message::AllFinished, 0, 0);
			};

			_workerThread = _calculator.CalculateChecksumsAsync(paths, std::move(callbacks));
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
	try
	{
		INITCOMMONCONTROLSEX initCtrl = { 0 };
		initCtrl.dwSize = sizeof(INITCOMMONCONTROLSEX);
		initCtrl.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES;

		if (!InitCommonControlsEx(&initCtrl))
		{
			throw std::system_error(GetLastError(), std::system_category(), "InitCommonControlsEx failed");
		}

		HashCalcGUI::MainWindow app;
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
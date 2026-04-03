#include "PCH.hpp"
#include "TiivisteMattiGUI.hpp"
#include "Resources.h"
#include "../Version.h"

import TiivisteMattiLib;

namespace TiivisteMatti
{
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

	void MainWindow::Create(HINSTANCE instance, int cmdShow)
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

	int MainWindow::Run()
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

	LRESULT CALLBACK MainWindow::WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
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

	LRESULT MainWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam)
	{
		try
		{
			switch (message)
			{
			case WM_CREATE:
				OnCreate();
				break;
			case WM_DESTROY:
			{
				OnDestroy();
				break;
			}
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
			case IDs::Message::UpdateProgress:
			{
				OnUpdateProgress(lparam);
				break;
			}
			case IDs::Message::UpdateComplete:
			{
				OnUpdateComplete(lparam);
				break;
			}
			case IDs::Message::UpdateError:
			{
				OnUpdateError(lparam);
				break;
			}
			case IDs::Message::Finished:
			{
				OnFinished();
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

	void MainWindow::CreateMainMenu()
	{
		HMENU menu = CreateMenu();
		HMENU fileMenu = CreatePopupMenu();
		HMENU exportMenu = CreatePopupMenu();
		HMENU helpMenu = CreatePopupMenu();

		AppendMenuW(fileMenu, MF_STRING, IDs::FileBrowse, UiStrings.at(IDs::MenuFileBrowse).c_str());
		AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(fileMenu, MF_STRING, IDs::FileExit, UiStrings.at(IDs::MenuFileExit).c_str());

		AppendMenuW(exportMenu, MF_STRING, IDs::FileExportCSV, UiStrings.at(IDs::MenuExportCSV).c_str());

		AppendMenuW(helpMenu, MF_STRING, IDs::HelpAbout, UiStrings.at(IDs::MenuHelpAbout).c_str());

		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), UiStrings.at(IDs::MenuFile).c_str());
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(exportMenu), UiStrings.at(IDs::MenuExport).c_str());
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), UiStrings.at(IDs::MenuHelp).c_str());

		SetMenu(_window, menu);
	}

	void MainWindow::CreateTreeView()
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

	void MainWindow::OnCreate()
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

	void MainWindow::OnDestroy()
	{
		DragAcceptFiles(_window, FALSE);

		for (auto& thread : _threads)
		{
			if (thread.joinable())
			{
				thread.request_stop();
			}
		}

		PostQuitMessage(0);
	}

	void MainWindow::ProcessPathsAsync(const std::vector<std::filesystem::path>& paths)
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

		callbacks.OnComplete = [this](const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& hashes)
		{
			auto data = new ResultData{ path, hashes, L"" };

			if (!PostMessageW(_window, IDs::Message::UpdateComplete, 0, reinterpret_cast<LPARAM>(data)))
			{
				delete data;
			}

			_results[path] = hashes;
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

		_threads.emplace_back(_calculator.CalculateChecksumsAsync(paths, std::move(callbacks)));
	}

	void MainWindow::OnSize(LPARAM lparam) const
	{
		int width = LOWORD(lparam);
		int height = HIWORD(lparam);

		SendMessageW(_statusBar, WM_SIZE, 0, 0);
		RECT statusRect;
		GetWindowRect(_statusBar, &statusRect);

		int statusHeight = statusRect.bottom - statusRect.top;
		MoveWindow(_treeView, 0, 0, width, height - statusHeight, TRUE);
	}

	LRESULT MainWindow::OnNotify(WPARAM wparam, LPARAM lparam) const
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

	void MainWindow::OnCommand(WORD id)
	{
		switch (id)
		{
		case IDs::FileBrowse:
			HandleBrowse();
			break;
		case IDs::FileExportCSV:
			HandleExport();
			break;
		case IDs::FileExit:
			SendMessageW(_window, WM_CLOSE, 0, 0);
			break;
		case IDs::HelpAbout:
		{
			HandleAbout();
			break;
		}
		}
	}

	void MainWindow::OnDropFiles(HDROP drop)
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

	void MainWindow::OnUpdateProgress(LPARAM lparam)
	{
		std::unique_ptr<ProgressData> data(reinterpret_cast<ProgressData*>(lparam));

		if (_progressNodes.find(data->FilePath) == _progressNodes.end())
		{
			AddFileToTree(data->FilePath);
			UpdateStatusBar(IDs::Message::UpdateProgress);
		}

		std::wstring text = std::vformat(UiStrings.at(IDs::TreeCalculating), std::make_wformat_args(data->Percentage));

		TVITEMW tvi = { 0 };
		tvi.mask = TVIF_TEXT;
		tvi.hItem = _progressNodes[data->FilePath];
		tvi.pszText = text.data();

		SendMessageW(_treeView, TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&tvi));
	}

	void MainWindow::OnUpdateComplete(LPARAM lparam)
	{
		std::unique_ptr<ResultData> data(reinterpret_cast<ResultData*>(lparam));

		if (_fileNodes.find(data->FilePath) == _fileNodes.end())
		{
			AddFileToTree(data->FilePath); // Failsafe for 0-byte files
		}

		FinalizeFileNode(data->FilePath, data->Hashes, L"");
		++_completedFiles;
		UpdateStatusBar(IDs::Message::UpdateComplete);
	}

	void MainWindow::OnUpdateError(LPARAM lparam)
	{
		std::unique_ptr<ResultData> data(reinterpret_cast<ResultData*>(lparam));

		if (_fileNodes.find(data->FilePath) == _fileNodes.end())
		{
			AddFileToTree(data->FilePath);
		}

		FinalizeFileNode(data->FilePath, {}, data->ErrorMessage);
		++_errorFiles;
		UpdateStatusBar(IDs::Message::UpdateError);
	}

	void MainWindow::UpdateStatusBar(UINT message)
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

	void MainWindow::OnFinished()
	{
		_ASSERT(_activeThreads == 0);
		_threads.clear();
		UpdateStatusBar(IDs::Message::Finished);
	}

	void MainWindow::HandleBrowse()
	{
		std::vector<wchar_t> buffer(0x100000, L'\0');

		OPENFILENAMEW ofn = { 0 };
		ofn.lStructSize = sizeof(ofn);
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

	void MainWindow::HandleExport() const
	{
		if (_results.empty())
		{
			MessageBoxW(_window, UiStrings.at(IDs::ErrorExportEmpty).c_str(), UiStrings.at(IDs::InfoTitle).c_str(), MB_OK | MB_ICONINFORMATION);
			return;
		}

		if (_activeThreads > 0)
		{
			MessageBoxW(_window, UiStrings.at(IDs::ErrorExportBusy).c_str(), UiStrings.at(IDs::WarningTitle).c_str(), MB_OK | MB_ICONWARNING);
			return;
		}

		constexpr DWORD BufferSize = 0x800;
		wchar_t buffer[BufferSize] = { 0 };
		OPENFILENAMEW ofn = { sizeof(ofn) };
		ofn.hwndOwner = _window;
		ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = BufferSize;
		ofn.lpstrDefExt = L"csv";
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

		if (!GetSaveFileNameW(&ofn))
		{
			return;
		}

		TiivisteMattiLib::Handle file(CreateFileW(
			ofn.lpstrFile,
			GENERIC_WRITE,
			0,
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr));

		if (!file.IsValid())
		{
			MessageBoxW(_window, UiStrings.at(IDs::ErrorFileOpen).c_str(), UiStrings.at(IDs::ErrorTitle).c_str(), MB_OK | MB_ICONERROR);
			return;
		}

		DWORD bytesWritten = 0;
		
		const wchar_t bom = 0xFEFF;
		if (!WriteFile(file, &bom, sizeof(wchar_t), &bytesWritten, nullptr))
		{
			MessageBoxW(_window, UiStrings.at(IDs::ErrorFileWrite).c_str(), UiStrings.at(IDs::ErrorTitle).c_str(), MB_OK | MB_ICONERROR);
			return;
		}

		std::wstring data = std::format(L"Path;{}\r\n", TiivisteMattiLib::Strings::Join(DefaultAlgorithms, L','));

		for (const auto [path, hashes] : _results)
		{
			const auto hashValues = DefaultAlgorithms | std::views::transform([&hashes](const std::wstring& algo)
			{
				auto it = hashes.find(algo);
				return it != hashes.end() ? it->second : L"Missing?";
			});

			data += std::format(L"{},{}\r\n", path.native(), TiivisteMattiLib::Strings::Join(hashValues, L','));
		}

		if (!WriteFile(file, data.c_str(), static_cast<DWORD>(data.length() * sizeof(wchar_t)), &bytesWritten, nullptr))
		{
			MessageBoxW(_window, UiStrings.at(IDs::ErrorFileWrite).c_str(), UiStrings.at(IDs::ErrorTitle).c_str(), MB_OK | MB_ICONERROR);
		}
	}

	void MainWindow::HandleAbout() const
	{
		TASKDIALOGCONFIG config = { 0 };
		config.cbSize = sizeof(TASKDIALOGCONFIG);
		config.hwndParent = _window;
		config.hInstance = GetModuleHandleW(nullptr);
		config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
		config.dwCommonButtons = TDCBF_OK_BUTTON;
		config.pszWindowTitle = UiStrings.at(IDs::AboutTitle).c_str();
		config.pszMainInstruction = UiStrings.at(IDs::WindowTitle).c_str();
		config.pszMainIcon = MAKEINTRESOURCEW(1);
		const std::wstring wideVersion = TiivisteMattiLib::Strings::ToWide(TIIVISTEMATTI_VERSION);
		const std::wstring wideHash = TiivisteMattiLib::Strings::ToWide(TIIVISTEMATTI_COMMIT_HASH);

		std::wstring content = std::vformat(UiStrings.at(IDs::AboutText), std::make_wformat_args(wideVersion, wideHash));

		config.pszContent = content.c_str();
		config.pfCallback = [](HWND hwnd, UINT msg, WPARAM, LPARAM lParam, LONG_PTR) -> HRESULT
		{
			if (msg == TDN_HYPERLINK_CLICKED)
			{
				const wchar_t* url = reinterpret_cast<const wchar_t*>(lParam);

				if (!url)
				{
					return S_OK;
				}

				ShellExecuteW(hwnd, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
			}

			return S_OK;
		};

		TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
	}

	int MainWindow::SystemIconIndex(const std::filesystem::path& path, bool isFolder)
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

	HTREEITEM MainWindow::InsertTreeItem(const TVINSERTSTRUCTW& is) const
	{
		LRESULT result = SendMessageW(_treeView, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&is));
		return reinterpret_cast<HTREEITEM>(result);
	}

	void MainWindow::AddFileToTree(const std::filesystem::path& filePath)
	{
		const std::filesystem::path folderPath = filePath.parent_path();
		HTREEITEM folderItem = nullptr;
		bool isNewFolder = false;

		const auto it = _folderNodes.find(folderPath);

		if (it == _folderNodes.end())
		{
			int folderIcon = SystemIconIndex(folderPath, true);

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

		int fileIcon = SystemIconIndex(filePath, false);

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

	void MainWindow::InsertHashNodes(HTREEITEM parent, const std::map<std::wstring, std::wstring>& hashes) const
	{
		int index = 0;

		for (const auto& [algo, hash] : hashes)
		{
			TVINSERTSTRUCTW is = { 0 };
			is.hParent = parent;
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

	void MainWindow::InsertErrorNode(HTREEITEM parent, const std::wstring& error) const
	{
		TVINSERTSTRUCTW is = { 0 };
		is.hParent = parent;
		is.hInsertAfter = TVI_LAST;
		is.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		is.item.iImage = I_IMAGENONE;
		is.item.iSelectedImage = I_IMAGENONE;

		std::wstring errText = std::format(L"{}: {}", UiStrings.at(IDs::StatusError), error);
		is.item.pszText = errText.data();

		InsertTreeItem(is);
	}

	void MainWindow::FinalizeFileNode(const std::filesystem::path& filePath, const std::map<std::wstring, std::wstring>& hashes, const std::wstring& error)
	{
		auto progIt = _progressNodes.find(filePath);
		auto fileIt = _fileNodes.find(filePath);

		if (progIt == _progressNodes.end() || fileIt == _fileNodes.end())
		{
			return;
		}

		SendMessageW(_treeView, TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(progIt->second));
		_progressNodes.erase(progIt);

		if (!error.empty())
		{
			InsertErrorNode(fileIt->second, error);
		}
		else
		{
			InsertHashNodes(fileIt->second, hashes);
		}
	}

	void MainWindow::CopyHashToClipboard(HTREEITEM hItem) const
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

	void MainWindow::OnTreeViewContextMenu(LPARAM lparam) const
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
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int cmdShow)
{
	constexpr wchar_t ProductName[] = L"Tiiviste-Matti";

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
		const std::wstring msg = 
			std::format(L"A system exception occurred. Error:\n{}\nCode: {}",
			TiivisteMattiLib::Strings::ToWide(e.what()),
			e.code().value());
		MessageBoxW(nullptr, msg.c_str(), ProductName, MB_OK | MB_ICONERROR);
		return e.code().value();
	}
	catch (const std::exception& e)
	{
		const std::wstring msg = std::format(L"An exception occurred. Error:\n{}",
			TiivisteMattiLib::Strings::ToWide(e.what()));
		MessageBoxW(nullptr, msg.c_str(), ProductName, MB_OK | MB_ICONERROR);
		return ERROR_UNHANDLED_EXCEPTION;
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"An unknown exception occurred.", ProductName, MB_OK | MB_ICONERROR);
		return ERROR_UNHANDLED_EXCEPTION;
	}
}
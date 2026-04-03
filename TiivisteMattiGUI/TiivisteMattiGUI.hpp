#pragma once

#include "Resources.h"

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
			MenuExport = IDS_MenuExport,
			MenuExportCSV = IDS_MenuExportCSV,
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
			HelpAbout = 1003,
			FileExportCSV = 1004
		};

		enum Control : UINT_PTR
		{
			TreeView = 2001,
			StatusBar = 2002
		};
	};

	inline const std::initializer_list<std::wstring> DefaultAlgorithms = { L"MD5", L"SHA1", L"SHA256" };

	class MainWindow
	{
	public:
		void Create(HINSTANCE instance, int cmdShow);
		int Run();

	private:
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
		LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

		void OnCreate();
		void CreateMainMenu();
		void CreateTreeView();
		void OnDestroy();

		void ProcessPathsAsync(const std::vector<std::filesystem::path>& paths);

		void OnSize(LPARAM lparam) const;
		LRESULT OnNotify(WPARAM wparam, LPARAM lparam) const;
		void OnCommand(WORD id);
		void OnDropFiles(HDROP drop);
		void OnUpdateProgress(LPARAM lparam);
		void OnUpdateComplete(LPARAM lparam);
		void OnUpdateError(LPARAM lparam);
		void UpdateStatusBar(UINT message);
		void OnFinished();

		void HandleBrowse();
		void HandleExport() const;
		void HandleAbout() const;

		int SystemIconIndex(const std::filesystem::path& path, bool isFolder);
		HTREEITEM InsertTreeItem(const TVINSERTSTRUCTW& is) const;
		void AddFileToTree(const std::filesystem::path& filePath);
		void InsertHashNodes(HTREEITEM parent, const std::map<std::wstring, std::wstring>& hashes) const;
		void InsertErrorNode(HTREEITEM parent, const std::wstring& error) const;
		void FinalizeFileNode(const std::filesystem::path& filePath, const std::map<std::wstring, std::wstring>& hashes, const std::wstring& error);

		void CopyHashToClipboard(HTREEITEM hItem) const;
		void OnTreeViewContextMenu(LPARAM lparam) const;

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

		TiivisteMattiLib::Calculator _calculator{ DefaultAlgorithms };
		std::vector<std::jthread> _threads;
		std::atomic<int> _activeThreads{ 0 };
		std::mutex _mutex;

		std::map<std::filesystem::path, std::map<std::wstring, std::wstring>> _results;
	};
}
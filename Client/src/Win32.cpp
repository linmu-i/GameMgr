#include <Windows.h>

#include <Win32.h>

namespace win32
{
	void SharedWin32Handle::reset()
	{
		if (handle && handle != INVALID_HANDLE_VALUE && refCount)
		{
			if (refCount->fetch_sub(1) == 1)
			{
				::CloseHandle(handle);
			}
			handle = nullptr;
			refCount.reset();
		}
	}
    bool SharedWin32Handle::valid() const
    {
        return handle && handle != INVALID_HANDLE_VALUE;
	}

    SharedWin32Handle Win32CreateProcess(std::filesystem::path exePath, const std::string& args) {
        std::wstring cmdLine;
        if (!args.empty()) {
            int len = MultiByteToWideChar(CP_UTF8, 0, args.c_str(), static_cast<int>(args.size()), nullptr, 0);
            if (len == 0) return SharedWin32Handle(nullptr);
            cmdLine.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, args.c_str(), static_cast<int>(args.size()), cmdLine.data(), len);
        }

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (!::CreateProcessW(exePath.c_str(),
            cmdLine.empty() ? nullptr : cmdLine.data(),
            nullptr, nullptr, FALSE, 0, nullptr, exePath.parent_path().c_str(), &si, &pi)) {
            return SharedWin32Handle(nullptr);
        }
        ::CloseHandle(pi.hThread);
        return SharedWin32Handle(pi.hProcess);
    }

    bool IsProcessActive(const SharedWin32Handle& handle)
    {
        if (!handle.get()) return false;
        DWORD exitCode;
        if (!::GetExitCodeProcess(handle.get(), &exitCode))
        {
            return false;
        }
		return exitCode == STILL_ACTIVE;
    }
}
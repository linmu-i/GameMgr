#pragma once
#include <memory>
#include <atomic>
#include <filesystem>

namespace win32
{
	class SharedWin32Handle
	{
	private:
		void* handle;
		std::shared_ptr<std::atomic<int>> refCount;
		
	public:
		SharedWin32Handle(void* handle) : handle(handle), refCount(std::make_shared<std::atomic<int>>(1)) {}
		SharedWin32Handle(const SharedWin32Handle& other) : handle(other.handle), refCount(other.refCount)
		{
			refCount->fetch_add(1);
		}
		SharedWin32Handle(SharedWin32Handle&& other) noexcept : handle(other.handle), refCount(std::move(other.refCount))
		{
			other.handle = nullptr;
		}
		SharedWin32Handle& operator=(const SharedWin32Handle& other)
		{
			if (this != &other)
			{
				reset();
				handle = other.handle;
				refCount = other.refCount;
				refCount->fetch_add(1);
			}
			return *this;
		}
		SharedWin32Handle& operator=(SharedWin32Handle&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				handle = other.handle;
				refCount = std::move(other.refCount);
				other.handle = nullptr;
			}
			return *this;
		}
		void reset();
		~SharedWin32Handle() { reset(); }
		bool valid() const;
		void* get() const { return handle; }
	};

	SharedWin32Handle Win32CreateProcess(std::filesystem::path exePath, const std::string& args);
	bool IsProcessActive(const SharedWin32Handle& handle);
}
#pragma once

#include <unordered_map>
#include <ctime>
#include <string>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <EbbGlow/Utils/Serialization.h>

namespace type
{
	enum class FileStatus : uint8_t
	{
		Created = 0x0,
		Changed = 0x1,
		Deleted = 0x2
	};

	struct TableItem
	{
		int64_t time;
		FileStatus status;

		[[nodiscard]] bool operator==(const TableItem& other) const noexcept { return time == other.time; }
		[[nodiscard]] auto operator<=>(const TableItem& other) const noexcept { return time <=> other.time; }
	};

	class Table
	{
	public:
		using Directory = std::unordered_map<std::filesystem::path, std::unordered_map<std::filesystem::path, TableItem>>;

	private:
		Directory dat;
		int64_t lastForcedPushTime = 0;

	public:
		
		Directory& directory() { return dat; }
		const Directory& directory() const { return dat; }
		int64_t lastForcedPush() const { return lastForcedPushTime; }
		
		std::optional<TableItem> file(const std::filesystem::path& vDir);
		std::optional<TableItem> file(const std::filesystem::path& game, const std::filesystem::path& filename);

		std::vector<std::pair<std::filesystem::path, TableItem>> gameFiles(const std::filesystem::path& game);
		std::vector<std::pair<std::filesystem::path, TableItem>> allFiles();

		void insert(const std::filesystem::path& game, const std::filesystem::path& filename, TableItem item);
		void insert(const std::filesystem::path& vDir, TableItem item);

		//说明: 关于remove，仅会从本地表项移除，不影响其他端，因此可能出现删除后再次重新拉取的情况，若需要彻底删除，请进行强制推送
		void remove(const std::filesystem::path& game, const std::filesystem::path& filename);
		void remove(const std::filesystem::path& vDir);

		void forcedPush();
		void forcedPush(int64_t t) { lastForcedPushTime = t; }
	};

	template<ebbglow::utils::OutStream OS>
	bool Serialize(OS& os, const TableItem& item)
	{
		if (!ebbglow::utils::Serialize(os, item.time)) return false;
		if (!ebbglow::utils::Serialize(os, static_cast<uint8_t>(item.status))) return false;
		return true;
	}

	template<ebbglow::utils::InStream IS>
	bool Deserialize(IS& is, TableItem& item)
	{
		if (!ebbglow::utils::Deserialize(is, item.time)) return false;
		if (!ebbglow::utils::Deserialize(is, reinterpret_cast<uint8_t&>(item.status))) return false;
		return true;
	}

	template<ebbglow::utils::OutStream OS>
	bool Serialize(OS& os, const Table& table)
	{
		if (!ebbglow::utils::Serialize(os, table.lastForcedPush())) return false;
		if (!ebbglow::utils::Serialize(os, table.directory())) return false;
		return true;
	}

	template<ebbglow::utils::InStream IS>
	bool Deserialize(IS& is, Table& table)
	{
		int64_t tmp = 0;
		if (!ebbglow::utils::Deserialize(is, tmp)) return false;

		Table::Directory tmpDir;
		if (!ebbglow::utils::Deserialize(is, tmpDir)) return false;

		table.forcedPush(tmp);
		table.directory() = std::move(tmpDir);
		return true;
	}

	class MemoryOS
	{
	private:
		std::vector<char> buffer;
		size_t offset;

	public:
		MemoryOS() : offset(0) {}
		MemoryOS(size_t size) : buffer(size, 0), offset(0) {}
		void resize(size_t newSize) { buffer.resize(newSize); }
		void reset() { offset = 0; }

		bool write(const char* data, uint64_t size)
		{
			if (size == 0) return true;
			if (size + offset > buffer.size())
			{
				buffer.resize(std::max(static_cast<size_t>(offset + size), static_cast<size_t>(buffer.size() + buffer.size() / 2)));
			}
			memcpy(buffer.data() + offset, data, size);
			offset += size;
			return true;
		}
		bool write(std::span<const char> data) { return write(data.data(), data.size_bytes()); }

		std::vector<char>& data() { return buffer; }
		const std::vector<char>& data() const { return buffer; }

		std::span<char> activeData() { return std::span<char>{buffer.begin(), offset}; }
		std::span<const char> activeData() const { return std::span<const char>{buffer.begin(), offset}; }
	};

	class MemoryIS
	{
	private:
		std::span<const char> buffer;
		size_t offset;

	public:
		MemoryIS() = default;
		MemoryIS(std::span<const char> buffer) : buffer(buffer), offset(0) {}
		
		void reset(std::span<const char> buffer) { this->buffer = buffer; offset = 0; }

		bool read(char* buf, uint64_t size)
		{
			if (size == 0) return true;
			if (offset + size > buffer.size_bytes()) return false;
			memcpy(buf, buffer.data() + offset, size);
			offset += size;
			return true;
		}

		size_t position() const { return offset; }
	};
}
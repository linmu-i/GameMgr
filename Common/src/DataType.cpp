#include <TideEcho.h>

#include <DataType.h>

namespace data
{
	std::optional<type::Table> TableDataGetTable(std::span<const uint8_t> pkg)
	{
		if (GetDataType(pkg) != DataType::TableData) return std::nullopt;
		type::Table result;
		type::MemoryIS is{ pkg.subspan(4) };
		if (!type::Deserialize(is, result)) return std::nullopt;
		return result;
	}

	bool WriteFileData(const FileInfo& info, std::span<const uint8_t> fileData, std::ofstream& f)
	{
		if (!f.is_open() || !f.good()) return false;
		f.seekp(static_cast<std::streamoff>(info.maxPackageSize) * info.index, std::ios::beg);
		f.write(reinterpret_cast<const char*>(fileData.data()), fileData.size_bytes());
		return f.good();
	}

	bool WriteFileData(std::span<const uint8_t> pkg, std::ofstream& f)
	{
		if (GetDataType(pkg) != DataType::FileData)
		{
			return false;
		}
		type::MemoryIS is{ pkg.subspan(4) };
		FileInfo info;
		if (!Deserialize(is, info))
		{
			return false;
		}
		return WriteFileData(info, pkg.subspan(static_cast<size_t>(4) + is.position()), f);
	}

	std::optional<std::vector<uint8_t>> GetFilePiece(const FileInfo& info, std::ifstream& f)
	{
		if (!f.is_open() || !f.good()) return std::nullopt;
		auto tmpPos = f.tellg();
		f.seekg(0, std::ios::end);
		uint64_t fileSize = f.tellg();

		uint64_t offset = static_cast<uint64_t>(info.index) * info.maxPackageSize;
		if (offset >= fileSize) return std::nullopt;

		f.seekg(offset, std::ios::beg);
		if (!f.good()) return std::nullopt;

		uint64_t dataSize = std::min<uint64_t>(fileSize - offset, info.maxPackageSize);
		std::vector<uint8_t> result;
		result.resize(dataSize);
		f.read(reinterpret_cast<char*>(result.data()), dataSize);
		if (f.fail()) return std::nullopt;
		return result;
	}

	std::filesystem::path FileRequestGetVDir(std::span<const uint8_t> pkg)
	{
		if (GetDataType(pkg) != DataType::FileRequest) return "";
		std::filesystem::path result;
		type::MemoryIS is(pkg.subspan(4));
		if (!ebbglow::utils::Deserialize(is, result)) return "";
		return result;
	}

	FileInfo FilePieceRequestGetInfo(std::span<const uint8_t> pkg)
	{
		if (GetDataType(pkg) != DataType::FilePieceRequest) return {};
		type::MemoryIS is{ pkg.subspan(4) };
		FileInfo result;
		if (!Deserialize(is, result)) return {};
		return result;
	}

	FileInfo FileDataGetInfo(std::span<const uint8_t> pkg)
	{
		if (GetDataType(pkg) != DataType::FileData) return {};
		type::MemoryIS is{ pkg.subspan(4) };
		FileInfo result;
		if (!Deserialize(is, result)) return {};
		return result;
	}

	std::u8string ErrorGetReason(std::span<const uint8_t> pkg)
	{
		if (GetDataType(pkg) != DataType::Error) return u8"";
		std::u8string result;
		type::MemoryIS is{ pkg.subspan(4) };
		if (!ebbglow::utils::Deserialize(is, result)) return u8"";
		return result;
	}

	std::vector<uint8_t> MakeTableRequest()
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::TableRequest);
		return result;
	}

	std::vector<uint8_t> MakeTableData(const type::Table& table)
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::TableData);
		type::MemoryOS os;
		if (!type::Serialize(os, table)) return {};
		result.insert(result.end(), os.activeData().begin(), os.activeData().end());
		return result;
	}

	std::vector<uint8_t> MakeFileRequest(const std::filesystem::path& vDir)
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::FileRequest);
		type::MemoryOS os;
		ebbglow::utils::Serialize(os, vDir);
		result.insert(result.end(), os.activeData().begin(), os.activeData().end());
		return result;
	}
	std::vector<uint8_t> MakeFileData(const FileInfo& info, std::ifstream& file)
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::FileData);

		type::MemoryOS os;
		Serialize(os, info);
		auto fileData = GetFilePiece(info, file);
		if (fileData)
		{
			result.insert(result.end(), os.activeData().begin(), os.activeData().end());
			result.insert(result.end(), fileData->begin(), fileData->end());
			return result;
		}
		else
		{
			return {};
		}
	}

	std::vector<uint8_t> MakeFileData(const FileInfo& info, std::span<const uint8_t> filePieceData)
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::FileData);
		type::MemoryOS os;
		Serialize(os, info);
		result.insert(result.end(), os.activeData().begin(), os.activeData().end());
		result.insert(result.end(), filePieceData.begin(), filePieceData.end());
		return result;
	}

	std::vector<uint8_t> MakeFilePieceRequest(const FileInfo& info)
	{
		type::MemoryOS os;
		Serialize(os, info);
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::FilePieceRequest);
		result.insert(result.end(), os.activeData().begin(), os.activeData().end());
		return result;
	}

	std::vector<uint8_t> MakeDeleteRequest(const std::filesystem::path& vDir)
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::DeleteRequest);
		type::MemoryOS os;
		ebbglow::utils::Serialize(os, vDir);
		result.insert(result.end(), os.activeData().begin(), os.activeData().end());
		return result;
	}

	std::filesystem::path DeleteRequestGetVDir(std::span<const uint8_t> pkg)
	{
		if (GetDataType(pkg) != DataType::DeleteRequest) return "";
		std::filesystem::path result;
		type::MemoryIS is(pkg.subspan(4));
		if (!ebbglow::utils::Deserialize(is, result)) return "";
		return result;
	}

	std::vector<uint8_t> MakeError(const std::u8string& reason)
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::Error);
		type::MemoryOS os;
		ebbglow::utils::Serialize(os, reason);
		result.insert(result.end(), os.activeData().begin(), os.activeData().end());
		return result;
	}

	std::vector<uint8_t> MakeACK()
	{
		std::vector<uint8_t> result;
		result.resize(4);
		result[0] = 0x66;
		result[1] = 0xcc;
		result[2] = 0xff;
		result[3] = static_cast<uint8_t>(DataType::ACK);
		return result;
	}
}
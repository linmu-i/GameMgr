#pragma once

//	数据包定义
//		3字节魔数0x66ccff
//		1字节类型
//			0x0 请求表
//			0x1 表数据
//			0x2 请求文件(虚拟路径)
//			0x3 文件分片数据(虚拟路径)
//			0x4 ACK
//			0x5 文件片请求(虚拟路径)
//			0xff Error
//		柔性数据
//			对于0x0 无柔性数据
//			对于0x1 为表数据(作为内存流进行反序列化)
//			对于0x2 虚拟路径
//			对于0x3 FileInfo的数据+柔性文件数据
//			对于0x4 无柔性数据
//			对于0x5 FileInfo的数据
//			对于0x6 虚拟路径
//			对于0xff 柔性数据为一个UTF-8字符串，表示原因，收到后立即断开本次会话，所有传输内容作废
//		注: 
//			虚拟路径采用对path.generic_u8string()的标准序列化，应为8字节无符号长度+utf-8数据
//			所有长度标识均视为小端序无符号整数，文件包数/编号/包长也为小端序无符号整数

#include <span>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <TideEcho.h>

#include <Type.h>

namespace data
{
	enum class DataType : uint8_t
	{
		TableRequest = 0x0,
		TableData = 0x1,
		FileRequest = 0x2,
		FileData = 0x3,
		ACK = 0x4,
		FilePieceRequest = 0x5,
		DeleteRequest = 0x6,
		Error = 0xff
	};

	[[nodiscard]] constexpr DataType GetDataType(std::span<const uint8_t> pkg) noexcept
	{
		if (pkg.size() < 4) return DataType::Error;
		if (pkg[0] != 0x66 ||
			pkg[1] != 0xcc ||
			pkg[2] != 0xff) return DataType::Error;
		return static_cast<DataType>(pkg[3]);
	}

	std::optional<type::Table> TableDataGetTable(std::span<const uint8_t> pkg);

	std::filesystem::path FileRequestGetVDir(std::span<const uint8_t> pkg);

	struct FileInfo
	{
		std::filesystem::path vDir;
		uint16_t index = 0;
		uint16_t packageCount = 0;
		uint32_t maxPackageSize = 0;
	};

	FileInfo FilePieceRequestGetInfo(std::span<const uint8_t> pkg);
	FileInfo FileDataGetInfo(std::span<const uint8_t> pkg);
	std::filesystem::path DeleteRequestGetVDir(std::span<const uint8_t> pkg);

	std::u8string ErrorGetReason(std::span<const uint8_t> pkg);

	template<ebbglow::utils::OutStream OS>
	bool Serialize(OS& os, const FileInfo& info)
	{
		if (!ebbglow::utils::Serialize(os, info.packageCount)) return false;
		if (!ebbglow::utils::Serialize(os, info.index)) return false;
		if (!ebbglow::utils::Serialize(os, info.maxPackageSize)) return false;
		if (!ebbglow::utils::Serialize(os, info.vDir)) return false;
		return true;
	}

	template<ebbglow::utils::InStream IS>
	bool Deserialize(IS& is, FileInfo& info)
	{
		if (!ebbglow::utils::Deserialize(is, info.packageCount)) return false;
		if (!ebbglow::utils::Deserialize(is, info.index)) return false;
		if (!ebbglow::utils::Deserialize(is, info.maxPackageSize)) return false;
		if (!ebbglow::utils::Deserialize(is, info.vDir)) return false;
		return true;
	}
	
	bool WriteFileData(const FileInfo& info, std::span<const uint8_t> fileData, std::ofstream& f);
	bool WriteFileData(std::span<const uint8_t> pkg, std::ofstream& f);

	std::optional<std::vector<uint8_t>> GetFilePiece(const FileInfo& info, std::ifstream& f);

	std::vector<uint8_t> MakeTableRequest();
	std::vector<uint8_t> MakeTableData(const type::Table& table);
	std::vector<uint8_t> MakeFileRequest(const std::filesystem::path& vDir);
	std::vector<uint8_t> MakeFileData(const FileInfo& info, std::ifstream& file);
	std::vector<uint8_t> MakeFileData(const FileInfo& info, std::span<const uint8_t> filePieceData);
	std::vector<uint8_t> MakeFilePieceRequest(const FileInfo& info);
	std::vector<uint8_t> MakeDeleteRequest(const std::filesystem::path& vDir);
	std::vector<uint8_t> MakeError(const std::u8string& reason);
	std::vector<uint8_t> MakeACK();
}
#pragma once

#include <Type.h>

namespace svr
{
	using DiffItem = std::pair<std::filesystem::path, type::TableItem>;
	void GetDiff(
		std::vector<DiffItem>& requestList,
		std::vector<DiffItem>& sendList,
		std::vector<std::pair<std::filesystem::path, int64_t>>& deleteList,
		std::vector<std::pair<std::filesystem::path, int64_t>>& deleteSendList,
		type::Table& svrTb, type::Table& cltTb);
}
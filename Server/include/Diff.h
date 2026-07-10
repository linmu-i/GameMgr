#pragma once

#include <Type.h>

namespace svr
{
	using DiffItem = std::pair<std::filesystem::path, type::TableItem>;
	void GetDiff(
		std::vector<DiffItem>& requestList,
		std::vector<DiffItem>& sendList,
		type::Table& svrTb, type::Table& cltTb);
}
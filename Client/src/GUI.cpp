#include <Global.h>

#include <GUI.h>

namespace gui
{
	void GUIMgr::update()
	{
		auto& panelTrans = *transPool->active()->get(panelId);
		auto& panelControl = *controlPool->active()->get(panelId);
		auto& panelView = *viewPortPool->active()->get(panelId);

		if (rebuildFlag)
		{
			rebuildFlag = false;
			for (auto& item : itemIds)
			{
				world->deleteUnit(item.gameNameTextBoxId);
				world->deleteUnit(item.gamePathTextBoxId);
				world->deleteUnit(item.startButtonId);
				world->getEntityManager()->recycleId(item.gameNameTextBoxId);
				world->getEntityManager()->recycleId(item.gamePathTextBoxId);
				world->getEntityManager()->recycleId(item.startButtonId);
			}
			itemIds.clear();
			gameExes.clear();
			int index = 0;
			for (auto& [gameName, gameCfg] : cfg->gameConfigs)
			{
				using namespace ebbglow::ui::yui;
				ItemId newItem;
				newItem.gameNameTextBoxId = world->getEntityManager()->getId();
				newItem.gamePathTextBoxId = world->getEntityManager()->getId();
				newItem.startButtonId = world->getEntityManager()->getId();
				itemIds.push_back(newItem);
				gameExes.push_back(gameCfg.gameExe);

				TransformCom nameTrans{ Transform{{70, 160 * index + 30}, {0,0}, 0.0f, 1.0f }, {} };
				ControlCom textControl{ {0, 0, 0, 0}, true, true, {} };
				ViewPortCom view{ std::nullopt, {} };

				TransformAttachTo(nameTrans, panelTrans, panelId);
				ControlAttachTo(textControl, panelControl, panelId);
				ViewPortAttachTo(view, panelView, panelId);

				ebbglow::Color textColor = 0x263290ff;

				world->createUnit(newItem.gameNameTextBoxId, nameTrans, textControl, view,
					TextBox
					{
						reinterpret_cast<const char*>(gameName.u8string().c_str()),
						48.0f,
						3.0f,
						ebbglow::utils::DynamicLoadFont(global::GetFontData(), reinterpret_cast<const char*>(gameName.u8string().c_str()), 48.0f),
						textColor
					},
					LayerCom{ &(*world->getUiLayer())[5] }
				);

				TransformCom pathTrans{ Transform{{70, 160 * index + 70}, {0,0}, 0.0f, 1.0f }, {} };
				TransformAttachTo(pathTrans, panelTrans, panelId);

				world->createUnit(newItem.gamePathTextBoxId, pathTrans, textControl, view,
					TextBox
					{
						reinterpret_cast<const char*>(gameCfg.gameExe.u8string().c_str()),
						24.0f,
						3.0f,
						ebbglow::utils::DynamicLoadFont(global::GetFontData(), reinterpret_cast<const char*>(gameCfg.gameExe.u8string().c_str()), 36.0f),
						textColor
					},
					LayerCom{ &(*world->getUiLayer())[5] }
				);

				TransformCom buttonTrans{ Transform{{50, 160 * index + 20}, {0,0}, 0.0f, 1.0f }, {} };
				ControlCom buttonControl{ {0, 0, 900, 120}, true, true, {} };

				TransformAttachTo(buttonTrans, panelTrans, panelId);
				ControlAttachTo(buttonControl, panelControl, panelId);

				world->getMessageManager()->subscribe(newItem.startButtonId);
				world->createUnit(newItem.startButtonId, buttonTrans, buttonControl, view,
					ButtonLogic{},
					FillButtonVisual
					{
						0xffffffaa,
						0x66ccffff,
						0x000000ff,
						ebbglow::utils::DynamicLoadFont(global::GetFontData(), "", 48.0f), 0.0f, 0.0f, ""
					},
					ButtonMessage{},
					LayerCom{ &(*world->getUiLayer())[4] }
				);

				++index;
			}
			auto& scrollContainer = *world->getDoubleBuffer<ebbglow::ui::yui::ScrollContainer>()->inactive()->get(scId);
			scrollContainer.minOffset = std::min(0.0f,  600.0f - 160 * static_cast<float>(itemIds.size()));
			scrollContainer.maxOffset = 0.0f;
			scrollContainer.offset = 0.0f;
		}

		{
			auto& iconTrans = *transPool->inactive()->get(loadingIconId);
			iconTrans.transform.rotation += ebbglow::utils::GetFrameTime() * 6.0f;
			iconTrans.transform.rotation = std::fmod(iconTrans.transform.rotation, std::numbers::pi_v<float> * 2.0f);
		}

		for (int i = 0; i < itemIds.size(); ++i)
		{
			auto& item = itemIds[i];
			auto msgList = world->getMessageManager()->getMessageList(item.startButtonId);
			if (msgList->empty()) continue;
			
			global::SyncFlag() = true;
			global::StartGameFlag() = { gameExes[i], true };
			syncCtxt->table = cfg::RebuildTable(*cfg);
			auto cmdRs = cmdMgr->pushCmd(::core::Command::Sync);
			global::SyncCmd() = std::move(cmdRs);
			int l = 1;
		}

		auto& inaPanelControl = *controlPool->inactive()->get(panelId);
		auto& inaLoadingPanelControl = *controlPool->inactive()->get(loadingPanelId);

		if (global::SyncFlag())
		{
			inaPanelControl.isActive = false;
			inaPanelControl.isVisible = false;
			inaLoadingPanelControl.isActive = true;
			inaLoadingPanelControl.isVisible = true;
			boardInterpolation += ebbglow::utils::GetFrameTime() * 10.0f;
			boardInterpolation = std::clamp(boardInterpolation, 0.0f, 0.6f);
		}
		else
		{
			inaPanelControl.isActive = true;
			inaPanelControl.isVisible = true;
			inaLoadingPanelControl.isActive = false;
			inaLoadingPanelControl.isVisible = false;
			boardInterpolation -= ebbglow::utils::GetFrameTime() * 10.0f;
			boardInterpolation = std::clamp(boardInterpolation, 0.0f, 0.6f);
		}

		if (global::SyncFlag() && (global::SyncCmd().success() || global::SyncCmd().error()))
		{
			global::SyncFlag() = false;
		}

		(*world->getUiLayer())[7].push_back(std::make_unique<BoardDraw>(boardInterpolation));
	}
}
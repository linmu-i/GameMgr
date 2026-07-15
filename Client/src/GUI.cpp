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

				float itemHeight = 120.0f;
				float itemSpacing = 5.0f;

				ItemId newItem;
				newItem.gameNameTextBoxId = world->getEntityManager()->getId();
				newItem.gamePathTextBoxId = world->getEntityManager()->getId();
				newItem.startButtonId = world->getEntityManager()->getId();
				itemIds.push_back(newItem);
				gameExes.push_back(gameCfg.gameExe);

				TransformCom nameTrans{ Transform{{70, (itemHeight + itemSpacing) * index + 30}, {0,0}, 0.0f, 1.0f }, {} };
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
						1.0f,
						ebbglow::utils::DynamicLoadFont(global::GetFontData(), reinterpret_cast<const char*>(gameName.u8string().c_str()), 48.0f),
						textColor
					},
					LayerCom{ &(*world->getUiLayer())[5] }
				);

				TransformCom pathTrans{ Transform{{70, (itemHeight + itemSpacing) * index + 70}, {0,0}, 0.0f, 1.0f }, {} };
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

				TransformCom buttonTrans{ Transform{{50, (itemHeight + itemSpacing) * index}, {0,0}, 0.0f, 1.0f }, {} };
				ControlCom buttonControl{ {0, 0, 800, itemHeight}, true, true, {} };

				TransformAttachTo(buttonTrans, panelTrans, panelId);
				ControlAttachTo(buttonControl, panelControl, panelId);

				world->getMessageManager()->subscribe(newItem.startButtonId);
				world->createUnit(newItem.startButtonId, buttonTrans, buttonControl, view,
					ButtonLogic{},
					FillButtonVisual
					{
						0xdddddd90,
						0x66ccff00,
						0x000000ff,
						ebbglow::utils::DynamicLoadFont(global::GetFontData(), "", 48.0f), 0.0f, 0.0f, "", 0.0f, 0.0f
					},
					ButtonMessage{},
					LayerCom{ &(*world->getUiLayer())[4] }
				);

				++index;
			}
			auto& scrollContainer = *world->getDoubleBuffer<ebbglow::ui::yui::ScrollContainer>()->inactive()->get(scId);
			scrollContainer.minOffset = std::min(0.0f,  605.0f - 125 * static_cast<float>(itemIds.size()));
			scrollContainer.maxOffset = 0.0f;
			scrollContainer.offset = 0.0f;
		}

		for (int i = 0; i < itemIds.size(); ++i)
		{
			auto& item = itemIds[i];
			auto msgList = world->getMessageManager()->getMessageList(item.startButtonId);
			if (!msgList) continue;
			std::error_code ec;
			if (!std::filesystem::is_regular_file(gameExes[i], ec)) continue;
			for (auto* msg : *msgList)
			{
				if (msg->getType() == buttonReleaseMsgType)
				{
					global::SyncFlag() = true;
					global::StartGameFlag() = { gameExes[i], true };
					syncCtxt->table = cfg::RebuildTable(*cfg);
					rebuildStatus(StatusType::Syncing, "");
					auto cmdRs = cmdMgr->pushCmd(::core::Command::Sync);
					global::SyncCmd() = std::move(cmdRs);
				}
			}
			
		}

		if (rebuildSvrInfoFlag)
		{
			rebuildSvrInfoFlag = false;
			for (auto& id : svrInfoIds)
			{
				world->deleteUnit(id);
				world->getEntityManager()->recycleId(id);
			}
			svrInfoIds.clear();
			reconnectBtnId = ebbglow::core::InvalidEntity;

			using namespace ebbglow;

			ebbglow::core::entity svrInfoTitleId = world->getEntityManager()->getId();
			svrInfoIds.push_back(svrInfoTitleId);
			ui::yui::TransformCom titleTrans{ ui::yui::Transform{{95, 70}, {0,0}, 0.0f, 1.0f }, {} };
			world->createUnit(svrInfoTitleId, titleTrans,
				ui::yui::TextBox
				{
					.text = "服务器信息: ",
					.textSize = 36.0f,
					.spacing = 1.0f,
					.font = utils::DynamicLoadFont(global::GetFontData(), "服务器信息: ", 36.0f),
					.textColor = 0x263290ff
				},
				ui::yui::LayerCom{ &(*world->getUiLayer())[5] }
			);

			ebbglow::core::entity svrIpId = world->getEntityManager()->getId();
			svrInfoIds.push_back(svrIpId);
			ui::yui::TransformCom ipTrans{ ui::yui::Transform{{95, 120}, {0,0}, 0.0f, 1.0f }, {} };
			std::string ipText = "IP: " + syncCtxt->serverEndpoint.toString();
			world->createUnit(svrIpId, ipTrans,
				ui::yui::TextBox
				{
					.text = ipText,
					.textSize = 32.0f,
					.spacing = 1.0f,
					.font = utils::DynamicLoadFont(global::GetFontData(), ipText, 32.0f),
					.textColor = 0x263290ff
				},
				ui::yui::LayerCom{ &(*world->getUiLayer())[5] }
			);

			ebbglow::core::entity svrStatusId = world->getEntityManager()->getId();
			svrInfoIds.push_back(svrStatusId);
			ui::yui::TransformCom statusTrans{ ui::yui::Transform{{95, 170}, {0,0}, 0.0f, 1.0f }, {} };
			std::string statusText = syncCtxt->isRunning ? "- 同步线程运行中" : "- 同步线程已停止";
			world->createUnit(svrStatusId, statusTrans,
				ui::yui::TextBox
				{
					.text = statusText,
					.textSize = 32.0f,
					.spacing = 1.0f,
					.font = utils::DynamicLoadFont(global::GetFontData(), statusText, 32.0f),
					.textColor = 0x263290ff
				},
				ui::yui::LayerCom{ &(*world->getUiLayer())[5] }
			);

			if (!syncCtxt->isRunning)
			{
				ebbglow::core::entity svrStartBtnId = world->getEntityManager()->getId();
				svrInfoIds.push_back(svrStartBtnId);
				world->getMessageManager()->subscribe(svrStartBtnId);
				reconnectBtnId = svrStartBtnId;
				ui::yui::TransformCom startBtnTrans{ ui::yui::Transform{{65, 255}, {0,0}, 0.0f, 1.0f }, {} };
				ui::yui::ControlCom startBtnControl{ {0, 0, 310, 50}, true, true, {} };
				world->createUnit(svrStartBtnId, startBtnTrans, startBtnControl,
					ui::yui::ButtonLogic{},
					ui::yui::FillButtonVisual
					{
						.fillColor = 0xdddddd90,
						.borderColor = 0x66ccff00,
						.textColor = 0x263290ff,
						.font = ebbglow::utils::DynamicLoadFont(global::GetFontData(), "启动同步线程", 32.0f),
						.fontSize = 32.0f,
						.spacing = 1.0f,
						.text = "启动同步线程",
						.roundness = 0.0f,
					},
					ui::yui::ButtonMessage{},
					ui::yui::LayerCom{ &(*world->getUiLayer())[5] }
				);
			}
		}

		auto& inaPanelControl = *controlPool->inactive()->get(panelId);


		if (global::SyncFlag() || programRunning)
		{
			inaPanelControl.isActive = false;
		}
		else
		{
			inaPanelControl.isActive = true;
		}

		if (global::SyncFlag() && (global::SyncCmd().success() || global::SyncCmd().error()))
		{
			global::SyncFlag() = false;
		}

		auto* reconnectBtnMsgList = world->getMessageManager()->getMessageList(reconnectBtnId);
		if (reconnectBtnMsgList && !syncCtxt->isRunning)
		{
			for (auto* msg : *reconnectBtnMsgList)
			{
				if (msg->getType() == buttonReleaseMsgType)
				{
					syncCtxt->isRunning = true;
					thrPool->enqueue([this]() {::core::SyncMain(*cfg, *syncCtxt, *cmdMgr); });
				}
			}
		}


		if (rebuildStatusFlag != StatusType::None)
		{
			

			for (auto& id : statusIds)
			{
				world->deleteUnit(id);
				world->getEntityManager()->recycleId(id);
			}
			statusIds.clear();

			ebbglow::core::entity statusTitleId = world->getEntityManager()->getId();
			statusIds.push_back(statusTitleId);
			ebbglow::ui::yui::TransformCom titleTrans{ ebbglow::ui::yui::Transform{{95, 340}, {0,0}, 0.0f, 1.0f}, {} };
			world->createUnit(statusTitleId, titleTrans,
				ebbglow::ui::yui::TextBox
				{
					.text = "状态:",
					.textSize = 32.0f,
					.spacing = 1.0f,
					.font = ebbglow::utils::DynamicLoadFont(global::GetFontData(), "状态:", 32.0f),
					.textColor = 0x263290ff
				},
				ebbglow::ui::yui::LayerCom{ &(*world->getUiLayer())[5] }
			);

			ebbglow::core::entity statusTextId = world->getEntityManager()->getId();
			statusIds.push_back(statusTextId);
			ebbglow::ui::yui::TransformCom statusTrans{ ebbglow::ui::yui::Transform{{95, 380}, {0,0}, 0.0f, 1.0f}, {} };

			std::string statusText;
			if (rebuildStatusFlag == StatusType::Idle)
			{
				programRunning = false;
				statusText = "- 空闲";
			}
			else if (rebuildStatusFlag == StatusType::Syncing)
			{
				statusText = "- 同步中";
			}
			else if (rebuildStatusFlag == StatusType::ProgramRunning)
			{
				programRunning = true;
				statusText = "- 运行中: " + programName;
			}

			world->createUnit(statusTextId, statusTrans,
				ebbglow::ui::yui::TextBox
				{
					.text = statusText,
					.textSize = 32.0f,
					.spacing = 1.0f,
					.font = ebbglow::utils::DynamicLoadFont(global::GetFontData(), statusText, 32.0f),
					.textColor = 0x263290ff
				},
				ebbglow::ui::yui::LayerCom{ &(*world->getUiLayer())[5] }
			);

			rebuildStatusFlag = StatusType::None;
		}


		auto trans = ebbglow::ui::yui::GetFinalTransform(ebbglow::ui::yui::GetTransforms(*world, scId));
		(*world->getUiLayer())[2].push_back(std::make_unique<DrawRectDraw>(std::nullopt, ebbglow::Rect{ trans.position + ebbglow::Vec2{40, -10}, ebbglow::Vec2{820, 620} }, panelTrans.transform, 0xffffff50));
		(*world->getUiLayer())[2].push_back(std::make_unique<DrawRectDraw>(std::nullopt, ebbglow::Rect{ trans.position + ebbglow::Vec2{-305, -10}, ebbglow::Vec2{330, 620} }, panelTrans.transform, 0xffffff50));

		(*world->getUiLayer())[2].push_back(std::make_unique<DrawRectDraw>(std::nullopt, ebbglow::Rect{ trans.position + ebbglow::Vec2{-295, 0}, ebbglow::Vec2{310, syncCtxt->isRunning ? 240 : 180} }, panelTrans.transform, 0xdddddd90));
		(*world->getUiLayer())[2].push_back(std::make_unique<DrawRectDraw>(std::nullopt, ebbglow::Rect{ trans.position + ebbglow::Vec2{-295, 260}, ebbglow::Vec2{310, 150} }, panelTrans.transform, 0xdddddd90));

		(*world->getUiLayer())[7].push_back(std::make_unique<BoardDraw>(boardInterpolation));
	}

	void DrawRectDraw::draw()
	{
		ebbglow::gfx::DrawRect(rect, color);
	}
}
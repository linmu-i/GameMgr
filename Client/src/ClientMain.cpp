#include <EbbGlow/Utils/Control.h>
#include <EbbGlow/Graphics/Graphics.h>
#include <EbbGlow/UI/YUI/YUI.h>

#include <Global.h>
#include <GUI.h>
#include <Win32.h>
#include <Log.h>

int main()
{
	using namespace ebbglow;
	tideecho::NetServiceGuard netGuard;
	Init(1280, 720, "Game Manager");
	SetTargetFPS(120);

	global::GetFontData() = rsc::SharedFile(std::filesystem::path("font/SourceHanSansCN-Regular.otf"));

	ebbglow::core::World2D world{1280, 720};
	ui::yui::ApplyYUI(world);

	ebbglow::core::entity scId = world.getEntityManager()->getId();
	ebbglow::core::entity panelId = world.getEntityManager()->getId();

	ebbglow::ui::yui::TransformCom scTrans{ { {140, 60}, {0, 0}, 0.0f, 1.0f }, {} };
	ebbglow::ui::yui::ControlCom scControl{ {0, 0, 1000, 600}, true, true, {} };
	ebbglow::ui::yui::ViewPortCom scView{ Rect{0, 0, 1000, 600}, {} };

	ebbglow::ui::yui::TransformCom panelTrans{ { {0, 0}, {0, 0}, 0.0f, 1.0f }, {} };
	ebbglow::ui::yui::ControlCom panelControl{ {0, 0, 1000, 600}, true, true, {} };
	ebbglow::ui::yui::ViewPortCom panelView{ std::nullopt, {} };

	ebbglow::ui::yui::TransformAttachTo(panelTrans, scTrans, scId);
	ebbglow::ui::yui::ControlAttachTo(panelControl, scControl, scId);
	ebbglow::ui::yui::ViewPortAttachTo(panelView, scView, scId);

	world.createUnit(scId, scTrans, scControl, scView,
		ebbglow::ui::yui::ScrollContainer
		{
			.panelId = panelId,
			.height = 600,
			.maxOffset = 0,
			.minOffset = 0,
			.offset = 0,
			.speed = 30.0f
		});
	world.createUnit(panelId, panelTrans, panelControl, panelView);

	cfg::Config cfg = cfg::ReadConfig("config.ini");

	cfg.vDirToPhysicalPath = cfg::VDirToPhysicalPath{cfg};

	ui::yui::TransformCom backgroundTrans{ { {0, 0}, {0, 0}, 0.0f, 1.0f }, {} };
	ui::yui::ControlCom backgroundControl{ {0, 0, 1280, 720}, true, true, {} };
	ui::yui::ViewPortCom backgroundView{ std::nullopt, {} };

	world.createUnit(world.getEntityManager()->getId(), backgroundTrans, backgroundControl, backgroundView,
		ui::yui::ImageBox
		{
			ebbglow::rsc::SharedTexture(std::filesystem::path{"img/background.png"}),
		},
		ui::yui::LayerCom{ &(*world.getUiLayer())[0] }
	);

	ui::yui::TransformCom loadingPanelTrans{ { {0, 0}, {0, 0}, 0.0f, 1.0f }, {} };
	ui::yui::ControlCom loadingPanelControl{ {0, 0, 1280, 720}, true, true, {} };
	
	ui::yui::TransformCom loadingIconTrans{ { {400, 320}, {40, 40}, 0.0f, 1.0f }, {} };
	ui::yui::ControlCom loadingIconControl{ {0, 0, 1280, 720}, true, true, {} };

	ui::yui::TransformCom loadingTextTrans{ { {490, 320}, {40, 40}, 0.0f, 1.0f }, {} };
	ui::yui::ControlCom loadingTextControl{ {0, 0, 1280, 720}, true, true, {} };

	ebbglow::core::entity loadingPanelId = world.getEntityManager()->getId();
	ebbglow::core::entity loadingIconId = world.getEntityManager()->getId();
	ebbglow::core::entity loadingTextId = world.getEntityManager()->getId();


	ui::yui::TransformAttachTo(loadingIconTrans, loadingPanelTrans, loadingPanelId);
	ui::yui::ControlAttachTo(loadingIconControl, loadingPanelControl, loadingPanelId);
	ui::yui::TransformAttachTo(loadingTextTrans, loadingPanelTrans, loadingPanelId);
	ui::yui::ControlAttachTo(loadingTextControl, loadingPanelControl, loadingPanelId);

	world.createUnit(loadingPanelId, loadingPanelTrans, loadingPanelControl);
	world.createUnit(loadingIconId, loadingIconTrans, loadingIconControl, ui::yui::ImageBox{rsc::SharedTexture(std::filesystem::path{"img/Loading.png"})}, ui::yui::LayerCom{ &(*world.getUiLayer())[10] });

	world.createUnit(loadingTextId, loadingTextTrans, loadingTextControl,
		ui::yui::TextBox
		{ "同步中...", 48.0f, 4.0f, utils::DynamicLoadFont(global::GetFontData(), "同步中.", 64), 0x263290ff}, ui::yui::LayerCom{&(*world.getUiLayer())[10]}
	);

	::core::SyncContext syncContext;
	::core::CommandManager cmdMgr{ syncContext.isRunning };

	gui::GUIMgr guiMgr(world, cfg, panelId, scId, loadingPanelId, loadingIconId, cmdMgr, syncContext);
	guiMgr.rebuild();

	world.addSystem(std::move(guiMgr));

	syncContext.isRunning = true;
	syncContext.serverEndpoint = tideecho::NetEndpoint("127.0.0.1", 34184, tideecho::AddressFamily::IPv4);

	ebbglow::utils::ThreadPool threadPool(4);

	threadPool.enqueue([&syncContext, &cfg, &cmdMgr]()
	{
			::core::SyncMain(cfg, syncContext, cmdMgr);
		});

	win32::SharedWin32Handle win32Handle{ nullptr };
	std::atomic<bool> isProcessActive{ false };

	while (!WindowShouldClose())
	{
		if (!global::SyncFlag() && global::StartGameFlag().second && !isProcessActive)
		{
			win32Handle = win32::Win32CreateProcess(global::StartGameFlag().first, "");
			if (win32Handle.valid())
			{
				isProcessActive = true;
			}
			else
			{
				mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to start game process: {}", global::StartGameFlag().first.string());
			}
			global::StartGameFlag().second = false;
		}

		if (isProcessActive)
		{
			if (!win32::IsProcessActive(win32Handle))
			{
				isProcessActive = false;
				global::SyncFlag() = true;
				syncContext.table = cfg::RebuildTable(cfg);
				global::SyncCmd() = cmdMgr.pushCmd(::core::Command::Sync);
			}
		}

		cmdMgr.update();
		world.update();
		ebbglow::BeginDrawing();
		gfx::ClearBackground(0x00000000);
		world.draw();
		ebbglow::EndDrawing();
	}
	syncContext.isRunning = false;
}
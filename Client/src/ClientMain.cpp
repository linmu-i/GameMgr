#include <EbbGlow/Utils/Control.h>
#include <EbbGlow/Graphics/Graphics.h>
#include <EbbGlow/UI/YUI/YUI.h>

#include <Global.h>
#include <GUI.h>

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

	//scControl.isVisible = false;
	//scControl.isActive = false;
	//global::SyncFlag() = true;

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

	ebbglow::core::entity loadingPanelId = world.getEntityManager()->getId();
	ebbglow::core::entity loadingIconId = world.getEntityManager()->getId();

	ui::yui::TransformAttachTo(loadingIconTrans, loadingPanelTrans, loadingPanelId);
	ui::yui::ControlAttachTo(loadingIconControl, loadingPanelControl, loadingPanelId);

	world.createUnit(loadingPanelId, loadingPanelTrans, loadingPanelControl);
	world.createUnit(loadingIconId, loadingIconTrans, loadingIconControl, ui::yui::ImageBox{rsc::SharedTexture(std::filesystem::path{"img/Loading.png"})}, ui::yui::LayerCom{ &(*world.getUiLayer())[10] });

	gui::GUIMgr guiMgr(world, cfg, panelId, scId, loadingPanelId, loadingIconId);
	guiMgr.rebuild();

	world.addSystem(std::move(guiMgr));


	::core::SyncContext syncContext;
	::core::CommandManager cmdMgr;
	syncContext.isRunning = true;
	syncContext.serverEndpoint = tideecho::NetEndpoint("127.0.0.1", 34184, tideecho::AddressFamily::IPv4);

	ebbglow::utils::ThreadPool threadPool(4);

	threadPool.enqueue([&syncContext, &cfg, &cmdMgr]()
	{
			::core::SyncMain(cfg, syncContext, cmdMgr);
		});

	syncContext.table = cfg::RebuildTable(cfg);
	cmdMgr.pushCmd(::core::Command::Sync);

	while (!WindowShouldClose())
	{
		if (world.framesCount / 600 % 2 == 0)
		{
			//global::SyncFlag() = true;
		}
		else
		{
			//global::SyncFlag() = false;
		}
		world.update();
		BeginDrawing();
		gfx::ClearBackground(0x00000000);
		world.draw();
		EndDrawing();
	}
	syncContext.isRunning = false;
}
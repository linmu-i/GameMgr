#include <EbbGlow/UI/YUI/YUI.h>
#include <EbbGlow/Graphics/Graphics.h>

#include <Config.h>

namespace gui
{
	class BoardDraw : public ebbglow::core::DrawBase
	{
	private:
		float interpolation = 0.0f;
	public:
		BoardDraw(float interpolation) : interpolation(interpolation) {}
		void draw() override
		{
			auto sz = ebbglow::utils::ScreenSize();
			ebbglow::gfx::DrawRect({ {0,0}, sz }, { 255, 255, 255, static_cast<uint8_t>(255 * interpolation) });
		}
	};

	class GUIMgr : public ebbglow::core::SystemBase
	{
	private:
		ebbglow::core::World2D* world;
		cfg::Config* cfg;

		ebbglow::core::entity scId;
		ebbglow::core::entity panelId;

		ebbglow::core::entity loadingPanelId;
		ebbglow::core::entity loadingIconId;


		struct ItemId
		{
			ebbglow::core::entity gameNameTextBoxId;
			ebbglow::core::entity gamePathTextBoxId;
			ebbglow::core::entity startButtonId;
		};
		std::vector<ItemId> itemIds;
		std::vector<std::filesystem::path> gameExes;

		bool rebuildFlag = false;

		ebbglow::core::DoubleComs<ebbglow::ui::yui::TransformCom>* transPool;
		ebbglow::core::DoubleComs<ebbglow::ui::yui::ControlCom>* controlPool;
		ebbglow::core::DoubleComs<ebbglow::ui::yui::ViewPortCom>* viewPortPool;

		::core::CommandManager* cmdMgr;
		::core::SyncContext* syncCtxt;

		float boardInterpolation = 0.0f;

	public:
		GUIMgr(ebbglow::core::World2D& world, cfg::Config& cfg, ebbglow::core::entity panelId, ebbglow::core::entity scId,
			ebbglow::core::entity loadingPanelId, ebbglow::core::entity loadingIconId, ::core::CommandManager& cmdMgr,
			::core::SyncContext& syncCtxt) :
			
			world(&world), cfg(&cfg), panelId(panelId), scId(scId), loadingPanelId(loadingPanelId), loadingIconId(loadingIconId),
			transPool(world.getDoubleBuffer<ebbglow::ui::yui::TransformCom>()), controlPool(world.getDoubleBuffer<ebbglow::ui::yui::ControlCom>()),
			viewPortPool(world.getDoubleBuffer<ebbglow::ui::yui::ViewPortCom>()), cmdMgr(&cmdMgr), syncCtxt(&syncCtxt) {}
		void update() override;
		void rebuild() { rebuildFlag = true; }
	};

	
}
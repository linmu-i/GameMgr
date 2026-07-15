#include <EbbGlow/UI/YUI/YUI.h>
#include <EbbGlow/Graphics/Graphics.h>
#include <EbbGlow/UI/Button/ButtonMsg.h>

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

	class DrawRectDraw : public ebbglow::core::DrawBase
	{
	private:
		std::optional<ebbglow::Rect> vp;
		ebbglow::Rect rect;
		ebbglow::ui::yui::Transform trans;
		ebbglow::Color color;

	public:
		DrawRectDraw(std::optional<ebbglow::Rect> vp, ebbglow::Rect rect, ebbglow::ui::yui::Transform trans, ebbglow::Color color) : vp(vp), rect(rect), trans(trans), color(color) {}
		void draw() override;
	};

	struct ServerInfo
	{
		::core::SyncContext* syncCtxt;

	};

	enum class StatusType
	{
		Idle,
		Syncing,
		ProgramRunning,
		None
	};

	class GUIMgr : public ebbglow::core::SystemBase
	{
	private:
		ebbglow::core::World2D* world;
		cfg::Config* cfg;

		ebbglow::core::entity scId;
		ebbglow::core::entity panelId;

		ebbglow::core::entity reconnectBtnId;

		struct ItemId
		{
			ebbglow::core::entity gameNameTextBoxId;
			ebbglow::core::entity gamePathTextBoxId;
			ebbglow::core::entity startButtonId;
		};
		std::vector<ItemId> itemIds;
		std::vector<std::filesystem::path> gameExes;

		std::vector<ebbglow::core::entity> svrInfoIds;

		std::vector<ebbglow::core::entity> statusIds;

		bool rebuildFlag = false;
		bool rebuildSvrInfoFlag = false;
		StatusType rebuildStatusFlag = StatusType::None;
		std::string programName;

		bool programRunning = false;

		ebbglow::core::DoubleComs<ebbglow::ui::yui::TransformCom>* transPool;
		ebbglow::core::DoubleComs<ebbglow::ui::yui::ControlCom>* controlPool;
		ebbglow::core::DoubleComs<ebbglow::ui::yui::ViewPortCom>* viewPortPool;

		::core::CommandManager* cmdMgr;
		::core::SyncContext* syncCtxt;

		ebbglow::utils::ThreadPool* thrPool;

		float boardInterpolation = 0.0f;

		ebbglow::core::MessageTypeId buttonReleaseMsgType;

	public:
		GUIMgr(ebbglow::core::World2D& world, cfg::Config& cfg, ebbglow::core::entity panelId, ebbglow::core::entity scId,
			::core::CommandManager& cmdMgr, ::core::SyncContext& syncCtxt, ebbglow::utils::ThreadPool& thrPool) :
			world(&world), cfg(&cfg), panelId(panelId), scId(scId),
			transPool(world.getDoubleBuffer<ebbglow::ui::yui::TransformCom>()),
			controlPool(world.getDoubleBuffer<ebbglow::ui::yui::ControlCom>()),
			viewPortPool(world.getDoubleBuffer<ebbglow::ui::yui::ViewPortCom>()),
			buttonReleaseMsgType(world.getMessageManager()->getMessageTypeManager().getId<ebbglow::ui::ButtonReleaseMsg>()),
			cmdMgr(&cmdMgr), syncCtxt(&syncCtxt), thrPool(&thrPool) {}
		void update() override;
		void rebuild() { rebuildFlag = true; }
		void rebuildSvrInfo() { rebuildSvrInfoFlag = true; }
		void rebuildStatus(StatusType status, std::string programName) { rebuildStatusFlag = status; this->programName = std::move(programName); }
	};

	
}
#include <TideEcho.h>

int main()
{
	tideecho::TCPServer svr{ 11451 };
	std::deque<tideecho::NetPackage> activeQueue;
	std::deque<tideecho::NetPackage> waitQueue;
	tideecho::NetEndpoint activeRemote;

	while (1)
	{
		svr.update();

		auto pkg = svr.getPackage();
	}
}
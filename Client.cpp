#define OLC_PGE_APPLICATION
#include <iostream>
#include "networking/olc_net.h"
#include "olcPixelGameEngine.h"
#include "MessageTypes.h"
#include "helper/json.hpp"

using json = nlohmann::json;

class Client : public olc::net::client_interface<MsgTypes>
{
public:
	void PingServer()
	{
		olc::net::message<MsgTypes> msg;
		msg.header.id = MsgTypes::ServerPing;

		// Caution with this...
		std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();

		msg << timeNow;
		Send(msg);
	}

	void MessageAll()
	{
		olc::net::message<MsgTypes> msg;
		msg.header.id = MsgTypes::MessageAll;
		Send(msg);
	}

	void UpdateGameState(json updates)
	{
		olc::net::message<MsgTypes> msg;
		msg.header.id = MsgTypes::ClientUpdateGS;
		std::string s = updates.dump();
		uint32_t length = s.length();
		std::cout << length << "\n";
		for (char const &c : s)
		{
			msg << c;
		}
		Send(msg);
	}
};

class Example : public olc::PixelGameEngine
{
public:
	Example()
	{
		sAppName = "Example";
	}

public:
	bool OnUserCreate() override
	{
		// Called once at the start, so create things here
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		// called once per frame
		// for (int x = 0; x < ScreenWidth(); x++)
		// 	for (int y = 0; y < ScreenHeight(); y++)
		// 		Draw(x, y, olc::Pixel(rand() % 255, rand() % 255, rand()% 255));

		if (GetKey(olc::Key::A).bReleased)
			c.Connect("127.0.0.1", 60000);
		if (GetKey(olc::Key::S).bReleased)
			c.PingServer();

		json gs_updates;
		gs_updates["game_state1"] = "updated";
		gs_updates["game_state2"] = "updated2";
		if (GetKey(olc::Key::D).bReleased)
			c.UpdateGameState(gs_updates);

		if (c.IsConnected())
		{
			if (!c.Incoming().empty())
			{
				auto msg = c.Incoming().pop_front().msg;

				switch (msg.header.id)
				{
				case MsgTypes::ServerAccept:
				{
					// Server has responded to a ping request
					std::cout << "Server Accepted Connection\n";
				}
				break;

				case MsgTypes::ServerPing:
				{
					// Server has responded to a ping request
					std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
					std::chrono::system_clock::time_point timeThen;
					msg >> timeThen;
					std::cout << "Ping: " << std::chrono::duration<double>(timeNow - timeThen).count() << "\n";
				}
				break;

				case MsgTypes::ServerMessage:
				{
					// Server has responded to a ping request
					uint32_t clientID;
					msg >> clientID;
					std::cout << "Hello from [" << clientID << "]\n";
				}
				break;

				case MsgTypes::ServerACK:
				{
					// Server has ACKed
					std::cout << "Server ACKed\n";
				}
				break;
				}
			}
		}
		else
		{
			// std::cout << "Server Down\n";
		}

		return true;
	}

	Client c;
};

int main()
{
	Example demo;
	if (demo.Construct(256, 240, 1, 1))
		demo.Start();
	return 0;
}
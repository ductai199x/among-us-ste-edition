

#include <string>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <variant>
#include "networking/olc_net.h"
#include "MessageTypes.h"
#include "helper/json.hpp"

using json = nlohmann::json;

class GameInstance
{
public:
	GameInstance()
	{
		database["game_state1"] = 1;
		database["game_state2"] = 2.5;
		database["game_state3"] = {1, 2, 3, 4};
	}
	~GameInstance()
	{
		database.clear();
	}

	bool UpdateDatabase(json updates)
	{
		for (auto &[key, value] : updates.items())
		{
			std::cout << "Updating: " << key << " : " << value << std::endl;
			if (database.contains(key))
			{
				database[key] = value;
			}
			else
			{
				std::cout << key << " does not exist in database" << std::endl;
			}
		}
		return true;
	}

	void PrintDatabase()
	{
		std::cout << std::setw(4) << database << std::endl;
	}

private:
	json database;
};

class Server : public olc::net::server_interface<MsgTypes>
{
public:
	Server(uint16_t nPort) : olc::net::server_interface<MsgTypes>(nPort)
	{
	}

	bool CreateNewGameInstance()
	{
		gameInstance.PrintDatabase();

		return true;
	}

protected:
	virtual bool OnClientConnect(std::shared_ptr<olc::net::connection<MsgTypes>> client)
	{
		olc::net::message<MsgTypes> msg;
		msg.header.id = MsgTypes::ServerAccept;
		client->Send(msg);
		return true;
	}

	// Called when a client appears to have disconnected
	virtual void OnClientDisconnect(std::shared_ptr<olc::net::connection<MsgTypes>> client)
	{
		std::cout << "Removing client [" << client->GetID() << "]\n";
	}

	// Called when a message arrives
	virtual void OnMessage(std::shared_ptr<olc::net::connection<MsgTypes>> client, olc::net::message<MsgTypes> &msg)
	{
		switch (msg.header.id)
		{
		case MsgTypes::ServerPing:
		{
			std::cout << "[" << client->GetID() << "]: Server Ping\n";

			// Simply bounce message back to client
			client->Send(msg);
		}
		break;

		case MsgTypes::MessageAll:
		{
			std::cout << "[" << client->GetID() << "]: Message All\n";

			// Construct a new message and send it to all clients
			olc::net::message<MsgTypes> msg;
			msg.header.id = MsgTypes::ServerMessage;
			msg << client->GetID();
			MessageAllClients(msg, client);
		}
		break;

		case MsgTypes::ClientUpdateGS:
		{
			std::cout << "[" << client->GetID() << "]: ClientUpdateGS\n";
			std::vector<char> rcvBuf;
			uint32_t messageLength = msg.header.size;
			std::cout << messageLength << "\n";
			for (int i = 0; i < messageLength; i++)
			{
				char c;
				msg >> c;
				rcvBuf.push_back(c);
			}
			std::reverse(std::begin(rcvBuf), std::end(rcvBuf));
			std::string s(rcvBuf.begin(), rcvBuf.end());
			auto updates = json::parse(s);
			if (gameInstance.UpdateDatabase(updates))
			{
				gameInstance.PrintDatabase();
				olc::net::message<MsgTypes> ack;
				ack.header.id = MsgTypes::ServerACK;
				client->Send(ack);
			}
		}
		break;
		}
	}

private:
	GameInstance gameInstance;
};

int main()
{
	Server server(60000);
	server.Start();
	server.CreateNewGameInstance();

	while (1)
	{
		server.Update(-1, true);
	}

	return 0;
}
#define OLC_PGE_APPLICATION

#include "networking/olc_net.h"
#include "helper/helper.h"
#include "helper/olcPixelGameEngine.h"

using chrono_clock = std::chrono::steady_clock;

class Client : public olc::net::client_interface<MsgTypes> {
public:
    void PingServer() {
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::ServerPing;
        chrono_clock::time_point timeNow = chrono_clock::now();
        msg << timeNow;
        Send(msg);
    }

    void MessageAll() {
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::MessageAll;
        Send(msg);
    }

    void UpdateGameState(json &updates) {
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::ClientUpdateGS;
        msg.header.id = random();
        std::string s = updates.dump();
        for (auto it = s.crbegin(); it != s.crend(); ++it) {
            msg << *it;
        }
        Send(msg);
    }

    void AcceptSeverConnection() {
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::ClientAccept;
        Send(msg);
    }
};

class AmongUsSTE : public olc::PixelGameEngine {
public:
    AmongUsSTE() {
        sAppName = "Example";
    }

    bool OnUserCreate() override {
        // Called once at the start, so create things here

        // Start the network thread
        networkThread = std::thread([this]() {
            while (1) { handleNetworking(); }
        });

        netClient.Connect("127.0.0.1", 60000);

        return true;
    }

    bool OnUserUpdate(float fElapsedTime) override {
        // called once per frame

        if (GetKey(olc::Key::A).bReleased)
            netClient.Connect("127.0.0.1", 60000);
        if (GetKey(olc::Key::S).bReleased)
            netClient.PingServer();

//        json gs_updates;
//        gs_updates["game_state1"] = "updated";
//        gs_updates["game_state2"] = "updated2";
//        if (GetKey(olc::Key::D).bReleased)
//            netClient.UpdateGameState(gs_updates);

        std::cout << "lmao\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));

        return true;
    }

protected:
    void handleNetworking()
    {
        if (!netClient.IsConnected()) return;

        netClient.Incoming().wait();

        while (!netClient.Incoming().empty()) {
            auto incoming_msg = netClient.Incoming().pop_front().msg;
            switch (incoming_msg.header.type) {
                case MsgTypes::ServerAccept: {
                    std::cout << "Server Accepted Connection. Send back an acknowledgement.\n";
                    netClient.AcceptSeverConnection();
                    break;
                }
                case MsgTypes::ServerPing: {
                    chrono_clock::time_point timeNow = chrono_clock::now();
                    chrono_clock::time_point timeThen;
                    incoming_msg >> timeThen;
                    std::cout << "Ping: " << std::chrono::duration<double>(timeNow - timeThen).count() << "\n";
                    break;
                }
                case MsgTypes::ClientPing: {
                    netClient.Send(incoming_msg);
                    break;
                }
                case MsgTypes::ServerMessage: {
                    uint32_t clientID;
                    incoming_msg >> clientID;
                    std::cout << "Hello from [" << clientID << "]\n";
                    break;
                }
                case MsgTypes::ServerACK: {
                    std::cout << "Server ACKed\n";
                    break;
                }
                case MsgTypes::ServerUpdateGS: {
                    uint32_t messageLength = incoming_msg.header.size;
                    std::vector<char> rcvBuf;
                    char c;
                    for (int i = 0; i < messageLength; i++) {
                        incoming_msg >> c;
                        rcvBuf.push_back(c);
                    }
                    std::reverse(std::begin(rcvBuf), std::end(rcvBuf));
                    // Parse json
                    auto updates = json::parse(rcvBuf);
                    // Update the database
                    if (gameInstance.UpdateDatabase(updates)) {
                        gameInstance.PrintDatabase();
                    }
                    break;
                }
                case MsgTypes::ServerDeny:
                    break;
                case MsgTypes::ClientTextMessage:
                    break;
            }
        }
    }


private:
    Client netClient;
    GameInstance gameInstance;

    // asio context handles the data transfer...
//    asio::io_context networkContext;
    // ...but needs a thread of its own to execute its work commands
    std::thread networkThread;
};

int main() {
    AmongUsSTE game;
    if (game.Construct(256, 240, 1, 1))
        game.Start();
    return 0;
}
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
        timeNow = chrono_clock::now();
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

    chrono_clock::time_point timeNow;
};

class Example : public olc::PixelGameEngine {
public:
    Example() {
        sAppName = "Example";
    }

public:
    int j = 0;

    bool OnUserCreate() override {
        // Called once at the start, so create things here
        return true;
    }

    bool OnUserUpdate(float fElapsedTime) override {
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



        if (c.IsConnected()) {
            if (!c.Incoming().empty()) {
                auto incoming_msg = c.Incoming().pop_front().msg;

                switch (incoming_msg.header.type) {
                    case MsgTypes::ServerAccept: {
                        std::cout << "Server Accepted Connection. Send back an acknowledgement.\n";
                        c.AcceptSeverConnection();
                        break;
                    }
                    case MsgTypes::ServerPing: {
                        c.timeNow = chrono_clock::now();
                        chrono_clock::time_point timeThen;
                        incoming_msg >> timeThen;
                        std::cout << "Ping: " << std::chrono::duration<double>(c.timeNow - timeThen).count() << "\n";
                        break;
                    }
                    case MsgTypes::ClientPing: {
                        std:: cout << j << " LUL\n";
                        if (j < 4)
                            c.Send(incoming_msg);
                        j++;
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
                    case MsgTypes::ClientAccept:
                        break;
                    case MsgTypes::ClientDeny:
                        break;
                    case MsgTypes::MessageAll:
                        break;
                    case MsgTypes::ClientTextMessage:
                        break;
                    case MsgTypes::ClientUpdateGS:
                        break;
                }
            }
        } else {
            // std::cout << "Server Down\n";
        }

        return true;
    }

    Client c;

private:
    GameInstance gameInstance;

};

int main() {
    Example demo;
    if (demo.Construct(256, 240, 2, 2))
        demo.Start();
    return 0;
}

#include "networking/olc_net.h"
#include "helper/helper.h"

using json = nlohmann::json;

class Server : public olc::net::server_interface<MsgTypes> {
public:
    Server(uint16_t nPort) : olc::net::server_interface<MsgTypes>(nPort) {
    }

    bool CreateNewGameInstance() {
        gameInstance.PrintDatabase();
        return true;
    }

protected:
    virtual bool OnClientConnect(std::shared_ptr<olc::net::connection<MsgTypes>> client) {
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::ServerAccept;
        client->Send(msg);
        return true;
    }

    // Called when a client appears to have disconnected
    virtual void OnClientDisconnect(std::shared_ptr<olc::net::connection<MsgTypes>> client) {
        std::cout << "Removing client [" << client->GetID() << "]\n";
    }

    // Called when a message arrives
    virtual void OnMessage(std::shared_ptr<olc::net::connection<MsgTypes>> client, olc::net::message<MsgTypes> &msg) {
        switch (msg.header.type) {
            case MsgTypes::ServerPing: {
                std::cout << "[" << client->GetID() << "]: Server Ping\n";
                client->Send(msg);
                break;
            }

            case MsgTypes::MessageAll: {
                std::cout << "[" << client->GetID() << "]: Message All\n";
                olc::net::message<MsgTypes> msg;
                msg.header.type = MsgTypes::ServerMessage;
                msg << client->GetID();
                MessageAllClients(msg, client);
                break;
            }

            case MsgTypes::ClientUpdateGS: {
                std::cout << "[" << client->GetID() << "]: ClientUpdateGS\n";
                // Get message length
                uint32_t messageLength = msg.header.size;
                // Initialize a char buffer to store the message's chars
                std::vector<char> rcvBuf;
                // Initialize a new message to send to all other connecting clients
                olc::net::message<MsgTypes> msg_all;
                msg_all.header.type = MsgTypes::ServerUpdateGS;
                // Unpack message into the char buffer
                char c;
                for (int i = 0; i < messageLength; i++) {
                    msg >> c;
                    msg_all << c;
                    rcvBuf.push_back(c);
                }
                // Parse json
                auto updates = json::parse(rcvBuf);
                // Update the database
                if (gameInstance.UpdateDatabase(updates)) {
                    gameInstance.PrintDatabase();
                    olc::net::message<MsgTypes> ack;
                    ack.header.type = MsgTypes::ServerACK;
                    ack.header.id = msg.header.id;
                    client->Send(ack);
                    // Update game states of all other clients
                    MessageAllClients(msg_all, client);
                }
                break;
            }
        }
    }

private:
    GameInstance gameInstance;
};

int main() {
    Server server(60000);
    server.Start();
    server.CreateNewGameInstance();

    while (1) {
        server.Update(-1, true);
    }

    return 0;
}
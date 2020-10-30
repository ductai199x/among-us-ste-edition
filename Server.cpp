
#include "networking/olc_net.h"
#include "helper/helper.h"

using json = nlohmann::json;
using chrono_clock = std::chrono::steady_clock;

class Server : public olc::net::server_interface<MsgTypes> {
public:
    explicit Server(uint16_t nPort) : olc::net::server_interface<MsgTypes>(nPort) {
        CreateNewGameInstance();

        clientStatusLock = std::unique_lock<std::mutex>(clientStatusMux, std::defer_lock);

        // Make a dummy work that track the context's work :) so that the context never expires automatically
        timerDummyWork = asio::require(timerContext.get_executor(), asio::execution::outstanding_work.tracked);
        // Start the context on its own thread
        timerContextThread = std::thread([this]() { timerContext.run(); });
    }

    ~Server() override
    {
        // Stop the main thread's context m_threadContext
        Stop();

        // Stop the timer thread's context
        timerDummyWork = asio::any_io_executor();
        timerContext.stop();
        // End the thread
        if (timerContextThread.joinable()) timerContextThread.join();
    }

    bool CreateNewGameInstance() {
        gameInstance.PrintDatabase();
        return true;
    }

protected:

    void checkClientConnStatus(const std::shared_ptr<asio::steady_timer>& t, std::shared_ptr<olc::net::connection<MsgTypes>> client)
    {

        uint32_t u_ID = client->GetID();
        std::string s_ID = std::to_string(u_ID);
        ConnectionStatus status = clientStatus[s_ID][0];
        int count = clientStatus[s_ID][1];
        clientStatusLock.lock();
        switch (status) {
            case ConnectionStatus::Waiting: {
                if (count > MAX_TIME_CLIENT_ACCEPT) {
                    clientStatus[s_ID][0] = ConnectionStatus::Dead;
                    // If client timeout, disconnect from this client and remove it from the database
                    OnClientDisconnect(client);
                    client->Disconnect();
                    client.reset();
                    timersContainer.erase(u_ID);
                    connectionsContainer.erase(u_ID);
                } else {
                    count++;
                    clientStatus[s_ID][1] = count;
                    t->expires_at(t->expiry() + asio::chrono::seconds(1));
                    t->async_wait(std::bind(&Server::checkClientConnStatus, this, t, client));
                }
                break;
            }
            case ConnectionStatus::Live: {
                if (count > 0)
                    std::cout << "Client[" << u_ID << "] has not answered back " << count << " times\n";
                if (count > MAX_FAIL_PINGS) {
                    clientStatus[s_ID][0] = ConnectionStatus::Dead;
                    // If client timeout, disconnect from this client and remove it from the database
                    OnClientDisconnect(client);
                    client->Disconnect();
                    client.reset();
                    timersContainer.erase(u_ID);
                    connectionsContainer.erase(u_ID);
                } else {
                    t->expires_at(t->expiry() + asio::chrono::seconds(CLIENT_PING_INTERVAL));
                    t->async_wait(std::bind(&Server::checkClientConnStatus, this, t, client));
                    clientStatus[s_ID][1] = ++count;
                    pingClient(u_ID);
                }
                break;
            }
            case ConnectionStatus::Dead: {
                break;
            }
            case ConnectionStatus::Unstable: {
                break;
            }
        }
        clientStatusLock.unlock();
    }

    void pingClient(uint32_t clientID) {
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::ClientPing;
        chrono_clock::time_point timeNow = chrono_clock::now();
        msg << timeNow;
        connectionsContainer[clientID]->Send(msg);
    }

    virtual bool OnClientConnect(std::shared_ptr<olc::net::connection<MsgTypes>> client) {
        // Create a message saying we accept client's connection attempt
        olc::net::message<MsgTypes> msg;
        msg.header.type = MsgTypes::ServerAccept;
        client->Send(msg);
        uint32_t clientID = client->GetID();

        // Add new client ID to the client status json map. The value contains: ConnectionStatus, # of unsuccessful
        // pings, last recorded ping duration
        clientStatus[std::to_string(clientID)] = {ConnectionStatus::Waiting, 0, -1.0};
        // Create an asio::steady_timer and bind it to the io service
        std::shared_ptr<asio::steady_timer> newTimer =
                std::make_shared<asio::steady_timer>(timerContext, asio::chrono::seconds(1));
        // Add an entry to the timer container
        timersContainer.emplace(client->GetID(), newTimer);
        newTimer->async_wait(std::bind(&Server::checkClientConnStatus, this, newTimer, client));

        return true;
    }

    // Called when a client appears to have disconnected
    virtual void OnClientDisconnect(std::shared_ptr<olc::net::connection<MsgTypes>> client) {
        std::cout << "Removing client [" << client->GetID() << "]\n";
    }

    // Called when a message arrives
    virtual void OnMessage(std::shared_ptr<olc::net::connection<MsgTypes>> client, olc::net::message<MsgTypes> &incoming_msg) {
        uint32_t u_ID = client->GetID();
        std::string s_ID = std::to_string(u_ID);

        switch (incoming_msg.header.type) {
            case MsgTypes::ServerPing: {
                std::cout << "[" << u_ID << "]: Server Ping\n";
                client->Send(incoming_msg);
                break;
            }
            case MsgTypes::ClientPing: {
                chrono_clock::time_point timeNow = chrono_clock::now();
                chrono_clock::time_point timeThen;
                incoming_msg >> timeThen;
                double dur =  std::chrono::duration<double>(timeNow - timeThen).count();
                std::cout << "Ping [" << u_ID << "]: " << dur << "\n";
                clientStatusLock.lock();
                clientStatus[s_ID][0] = ConnectionStatus::Live;
                clientStatus[s_ID][1] = 0;
                clientStatus[s_ID][2] = dur;
                clientStatusLock.unlock();
                break;
            }
            case MsgTypes::MessageAll: {
                std::cout << "[" << u_ID << "]: Message All\n";
                olc::net::message<MsgTypes> new_msg_all;
                new_msg_all.header.type = MsgTypes::ServerMessage;
                new_msg_all << u_ID;
                MessageAllClients(new_msg_all, client);
                break;
            }
            case MsgTypes::ClientUpdateGS: {
                std::cout << "[" << u_ID << "]: ClientUpdateGS\n";
                // Get message length
                uint32_t messageLength = incoming_msg.header.size;
                // Initialize a char buffer to store the message's chars
                std::vector<char> rcvBuf;
                // Initialize a new message to send to all other connecting clients
                olc::net::message<MsgTypes> new_msg_all;
                new_msg_all.header.type = MsgTypes::ServerUpdateGS;
                // Unpack message into the char buffer
                char c;
                for (int i = 0; i < messageLength; i++) {
                    incoming_msg >> c;
                    new_msg_all << c;
                    rcvBuf.push_back(c);
                }
                // Parse json
                auto updates = json::parse(rcvBuf);
                // Update the database
                if (gameInstance.UpdateDatabase(updates)) {
                    gameInstance.PrintDatabase();
                    olc::net::message<MsgTypes> ack;
                    ack.header.type = MsgTypes::ServerACK;
                    ack.header.id = incoming_msg.header.id;
                    client->Send(ack);
                    // Update game states of all other clients
                    MessageAllClients(new_msg_all, client);
                }
                break;
            }
            case MsgTypes::ClientAccept: {
                std::cout << "[" << u_ID << "]: Client accepted connection. Handshake completed\n";
                clientStatusLock.lock();
                clientStatus[s_ID][0] = ConnectionStatus::Live;
                clientStatus[s_ID][1] = 0;
                clientStatusLock.unlock();
                break;
            }
            case MsgTypes::ClientDeny:
                break;

            case MsgTypes::ClientTextMessage:
                break;
        }
    }

private:
    GameInstance gameInstance;
    std::mutex clientStatusMux;
    std::unique_lock<std::mutex> clientStatusLock;
    json clientStatus;
    asio::io_context timerContext;
    std::thread timerContextThread;
    asio::any_io_executor timerDummyWork;
    std::unordered_map<uint32_t, std::shared_ptr<asio::steady_timer>> timersContainer;
};

int main() {
    Server server(60000);
    server.Start();

    while (1) {
        server.Update(-1, true);
    }

    return 0;
}
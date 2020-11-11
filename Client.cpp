#define OLC_PGE_APPLICATION

#include "networking/olc_net.h"
#include "helper/helper.h"
#include "helper/olcExtended.h"

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

//        netClient.Connect("127.0.0.1", 60000);

        rendAllWalls.Load(spriteSheetPath);
        world.Create(mapSize.x, mapSize.y);

        for (int y = 0; y < world.size.y; y++)
            for (int x = 0; x < world.size.x; x++)
            {
                world.GetCell({ x, y }).wall = false; // floor

            }

        std::cout << "LOADING MAP...\n";
        std::ifstream file(mapJsonPath);
        file >> mapDict;
        int32_t key_, x, y;
        for (auto& [key, value]: mapDict.items()) {
            if (!strcmp(key.c_str(), "mapSize")) continue;
            key_ = std::stoi(key, nullptr, 10);
            x = key_ / mapSize.x;
            y = key_ - x * mapSize.x;
            world.GetCell({x, y}).wall = value["type"] == 1;
            for (int faceid = 0; faceid < 6; faceid++) {
                world.GetCell({x, y}).id[faceid] = {value[olc::Face_s[faceid]][0], value[olc::Face_s[faceid]][1]};
            }
        }

        return true;
    }

private:
    std::string assetsPath = "/home/bigboy/1-workdir/among-us-ste-edition/assets/";
    std::string spriteSheetPath = assetsPath + "32x32.png";
    std::string mapJsonPath = assetsPath + "map.json";

    olc::World world;
	olc::Renderable rendSelect;
	olc::Renderable rendAllWalls;

	olc::vf2d vCameraPos = { 0.0f, 0.0f };
	float fCameraAngle = 0.0f;
	float fCameraAngleTarget = fCameraAngle;
	float fCameraPitch = 5.5f;
	float fCameraZoom = 16.0f;

	bool bVisible[6];

//    olc::vi2d
	olc::vi2d vCursor = { 0, 0 };
	olc::vi2d vTileCursor = { 0,0 };
	olc::vi2d vTileSize = { 32, 32 };
    olc::vi2d mapSize = {64, 64};

    Client netClient;
    GameInstance gameInstance;
    std::thread networkThread;

    json mapDict;

protected:
    std::array<olc::vec3d, 8> CreateCube(const olc::vi2d& vCell, const float fAngle, const float fPitch, const float fScale, const olc::vec3d& vCamera)
	{
		// Unit Cube
		std::array<olc::vec3d, 8> unitCube, rotCube, worldCube, projCube;
		unitCube[0] = { 0.0f, 0.0f, 0.0f };
		unitCube[1] = { fScale, 0.0f, 0.0f };
		unitCube[2] = { fScale, -fScale, 0.0f };
		unitCube[3] = { 0.0f, -fScale, 0.0f };
		unitCube[4] = { 0.0f, 0.0f, fScale };
		unitCube[5] = { fScale, 0.0f, fScale };
		unitCube[6] = { fScale, -fScale, fScale };
		unitCube[7] = { 0.0f, -fScale, fScale };

		// Translate Cube in X-Z Plane
		for (int i = 0; i < 8; i++)
		{
			unitCube[i].x += (vCell.x * fScale - vCamera.x);
			unitCube[i].y += -vCamera.y;
			unitCube[i].z += (vCell.y * fScale - vCamera.z);
		}

		// Rotate Cube in Y-Axis around origin
		float s = sin(fAngle);
		float c = cos(fAngle);
		for (int i = 0; i < 8; i++)
		{
			rotCube[i].x = unitCube[i].x * c + unitCube[i].z * s;
			rotCube[i].y = unitCube[i].y;
			rotCube[i].z = unitCube[i].x * -s + unitCube[i].z * c;
		}

		// Rotate Cube in X-Axis around origin (tilt slighly overhead)
		s = sin(fPitch);
		c = cos(fPitch);
		for (int i = 0; i < 8; i++)
		{
			worldCube[i].x = rotCube[i].x;
			worldCube[i].y = rotCube[i].y * c - rotCube[i].z * s;
			worldCube[i].z = rotCube[i].y * s + rotCube[i].z * c;
		}

		// Project Cube Orthographically - Full Screen Centered
		for (int i = 0; i < 8; i++)
		{
			projCube[i].x = worldCube[i].x + ScreenWidth() * 0.5f;
			projCube[i].y = worldCube[i].y + ScreenHeight() * 0.5f;
			projCube[i].z = worldCube[i].z;
		}

		return projCube;
	}

	void CalculateVisibleFaces(std::array<olc::vec3d, 8>& cube)
	{
		auto CheckNormal = [&](int v1, int v2, int v3)
		{
			olc::vf2d a = { cube[v1].x, cube[v1].y };
			olc::vf2d b = { cube[v2].x, cube[v2].y };
			olc::vf2d c = { cube[v3].x, cube[v3].y };
			return  (b - a).cross(c - a) > 0;
		};

		bVisible[olc::Face::Floor] = CheckNormal(4, 0, 1);
		bVisible[olc::Face::South] = CheckNormal(3, 0, 1);
		bVisible[olc::Face::North] = CheckNormal(6, 5, 4);
		bVisible[olc::Face::East] = CheckNormal(7, 4, 0);
		bVisible[olc::Face::West] = CheckNormal(2, 1, 5);
		bVisible[olc::Face::Top] = CheckNormal(7, 3, 2);
	}

	void GetFaceQuads(const olc::vi2d& vCell, const float fAngle, const float fPitch, const float fScale, 
                    const olc::vec3d& vCamera, std::vector<olc::sQuad>& render)
	{
		std::array<olc::vec3d, 8> projCube = CreateCube(vCell, fAngle, fPitch, fScale, vCamera);

		auto& cell = world.GetCell(vCell);

		auto MakeFace = [&](int v1, int v2, int v3, int v4, olc::Face f)
		{
			render.push_back({ projCube[v1], projCube[v2], projCube[v3], projCube[v4], cell.id[f] });
		};

		if (!cell.wall)
		{
			if (bVisible[olc::Face::Floor]) MakeFace(4, 0, 1, 5, olc::Face::Floor);
		}
		else
		{
			if (bVisible[olc::Face::South]) MakeFace(3, 0, 1, 2, olc::Face::South);
			if (bVisible[olc::Face::North]) MakeFace(6, 5, 4, 7, olc::Face::North);
			if (bVisible[olc::Face::East]) MakeFace(7, 4, 0, 3, olc::Face::East);
			if (bVisible[olc::Face::West]) MakeFace(2, 1, 5, 6, olc::Face::West);
			if (bVisible[olc::Face::Top]) MakeFace(7, 3, 2, 6, olc::Face::Top);
		}
	}

public:
    bool OnUserUpdate(float fElapsedTime) override {
        // Position camera in world		
        vCameraPos = { vCursor.x + 0.5f, vCursor.y + 0.5f };
        vCameraPos *= fCameraZoom;

        // Rendering
        std::array<olc::vec3d, 8> cullCube = CreateCube({ 0, 0 }, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y });
        CalculateVisibleFaces(cullCube);

        // 2) Get all visible sides of all visible "tile cubes"o
        std::vector<olc::sQuad> vQuads;
        for (int y = 0; y < world.size.y; y++)
            for (int x = 0; x < world.size.x; x++)
                GetFaceQuads({ x, y }, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y }, vQuads);

        // 3) Sort in order of depth, from farthest away to closest
        std::sort(vQuads.begin(), vQuads.end(), [](const olc::sQuad& q1, const olc::sQuad& q2)
        {
            float z1 = (q1.points[0].z + q1.points[1].z + q1.points[2].z + q1.points[3].z) * 0.25f;
            float z2 = (q2.points[0].z + q2.points[1].z + q2.points[2].z + q2.points[3].z) * 0.25f;
            return z1 < z2;
        });

        // 4) Iterate through all "tile cubes" and draw their visible faces
        Clear(olc::BLACK);
        for (auto& q : vQuads)
            DrawPartialWarpedDecal
                    (
                            rendAllWalls.Decal(),
                            { {q.points[0].x, q.points[0].y}, {q.points[1].x, q.points[1].y}, {q.points[2].x, q.points[2].y}, {q.points[3].x, q.points[3].y} },
                            q.tile,
                            vTileSize
                    );

        // 6) Draw selection "tile cube"
        vQuads.clear();
        GetFaceQuads(vCursor, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y }, vQuads);
        for (auto& q : vQuads)
            DrawWarpedDecal(rendSelect.Decal(), { {q.points[0].x, q.points[0].y}, {q.points[1].x, q.points[1].y}, {q.points[2].x, q.points[2].y}, {q.points[3].x, q.points[3].y} });

        // 7) Draw some debug info
        DrawStringDecal({ 0,0 }, "Cursor: " + std::to_string(vCursor.x) + ", " + std::to_string(vCursor.y), olc::YELLOW, { 0.5f, 0.5f });
        DrawStringDecal({ 0,8 }, "Angle: " + std::to_string(fCameraAngle) + ", " + std::to_string(fCameraPitch), olc::YELLOW, { 0.5f, 0.5f });
        DrawStringDecal({ 0,16 }, mapDict.dump(2), olc::YELLOW, { 0.5f, 0.5f });

        if (GetKey(olc::Key::W).bPressed) {
//            vCursor.y +=
        }

		// Graceful exit if user is in full screen mode
		return !GetKey(olc::Key::ESCAPE).bPressed;
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
};

int main() {
    AmongUsSTE game;
    if (game.Construct(640, 480, 2, 2, false))
        game.Start();
    return 0;
}
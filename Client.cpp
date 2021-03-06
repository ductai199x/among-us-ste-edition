#define OLC_PGE_APPLICATION

#include "networking/olc_net.h"
#include "helper/helper.h"
#include "helper/olcExtended.h"
#include "helper/olcCollision.h"

using chrono_clock = std::chrono::steady_clock;

enum class RenderLayer {
    Zero,
    Splash,
    OpeningBg,
    OpeningFg,
    LobbyMap,
    LobbyBg,
    LobbyFg,
    GameMap,
    GameBg,
    GameFg
};

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
        sAppName = "AmongUsSTE";

        // Start the network thread
        networkThread = std::thread([this]() {
            while (1) { handleNetworking(); }
        });

        //        netClient.Connect("127.0.0.1", 60000);
    }

protected:
    std::string assetsPath = "/home/bigboy/1-workdir/among-us-ste-edition/assets/";
    std::string spriteSheetPath = assetsPath + "32x32.png";
    std::string mapJsonPath = assetsPath + "map.json";

    uint8_t nLayerBackground;
    RenderLayer layerToRender;

    json mapDict;
    olc::World world;
    olc::Renderable rendSelect;
    olc::Renderable rendAllWalls;

    olc::vf2d vCameraPos = {0.0f, 0.0f};
    float fCameraAngle = 0.5f;
    float fCameraAngleTarget = fCameraAngle;
    float fCameraPitch = 5.5f;
    float fCameraZoom = 16.0f;

    bool bVisible[6];

    olc::vf2d vCharVel = {0, 0};
    olc::vf2d vCharPos = {0, 0};
    olc::vi2d vTileSize = {32, 32};
    olc::vi2d mapSize = {64, 64};

    Client netClient;
    GameInstance gameInstance;
    std::thread networkThread;

    std::vector<olc::aabb::rect> vCollisionRects = std::vector<olc::aabb::rect>(9);
    std::vector<olc::vi2d> surroundRect = {
            {-1, -1},
            {0,  -1},
            {1,  -1},
            {-1, 0},
            {1,  0},
            {-1, 1},
            {0,  1},
            {1,  1}
    };

    std::array<olc::vec3d, 8>
    CreateCube(const olc::vf2d &vCell, const float fAngle, const float fPitch, const float fScale,
               const olc::vec3d &vCamera) {
        // Unit Cube
        std::array<olc::vec3d, 8> unitCube{}, rotCube{}, worldCube{}, projCube{};
        unitCube[0] = {0.0f, 0.0f, 0.0f};
        unitCube[1] = {fScale, 0.0f, 0.0f};
        unitCube[2] = {fScale, -fScale, 0.0f};
        unitCube[3] = {0.0f, -fScale, 0.0f};
        unitCube[4] = {0.0f, 0.0f, fScale};
        unitCube[5] = {fScale, 0.0f, fScale};
        unitCube[6] = {fScale, -fScale, fScale};
        unitCube[7] = {0.0f, -fScale, fScale};

        // Translate Cube in X-Z Plane
        for (int i = 0; i < 8; i++) {
            unitCube[i].x += (vCell.x * fScale - vCamera.x);
            unitCube[i].y += -vCamera.y;
            unitCube[i].z += (vCell.y * fScale - vCamera.z);
        }

        // Rotate Cube in Y-Axis around origin
        float s = std::sin(fAngle);
        float c = std::cos(fAngle);
        for (int i = 0; i < 8; i++) {
            rotCube[i].x = unitCube[i].x * c + unitCube[i].z * s;
            rotCube[i].y = unitCube[i].y;
            rotCube[i].z = unitCube[i].x * -s + unitCube[i].z * c;
        }

        // Rotate Cube in X-Axis around origin (tilt slighly overhead)
        s = std::sin(fPitch);
        c = std::cos(fPitch);
        for (int i = 0; i < 8; i++) {
            worldCube[i].x = rotCube[i].x;
            worldCube[i].y = rotCube[i].y * c - rotCube[i].z * s;
            worldCube[i].z = rotCube[i].y * s + rotCube[i].z * c;
        }

        // Project Cube Orthographically - Full Screen Centered
        for (int i = 0; i < 8; i++) {
            projCube[i].x = worldCube[i].x + ScreenWidth() * 0.5f;
            projCube[i].y = worldCube[i].y + ScreenHeight() * 0.5f;
            projCube[i].z = worldCube[i].z;
        }

        return projCube;
    }

    void CalculateVisibleFaces(std::array<olc::vec3d, 8> &cube) {
        auto CheckNormal = [&](int v1, int v2, int v3) {
            olc::vf2d a = {cube[v1].x, cube[v1].y};
            olc::vf2d b = {cube[v2].x, cube[v2].y};
            olc::vf2d c = {cube[v3].x, cube[v3].y};
            return (b - a).cross(c - a) > 0;
        };

        bVisible[olc::Face::Floor] = CheckNormal(4, 0, 1);
        bVisible[olc::Face::South] = CheckNormal(3, 0, 1);
        bVisible[olc::Face::North] = CheckNormal(6, 5, 4);
        bVisible[olc::Face::East] = CheckNormal(7, 4, 0);
        bVisible[olc::Face::West] = CheckNormal(2, 1, 5);
        bVisible[olc::Face::Top] = CheckNormal(7, 3, 2);
    }

    void GetFaceQuads(const olc::vf2d &vCell, const float fAngle, const float fPitch, const float fScale,
                      const olc::vec3d &vCamera, std::vector<olc::sQuad> &render) {
        std::array<olc::vec3d, 8> projCube = CreateCube(vCell, fAngle, fPitch, fScale, vCamera);

        auto &cell = world.GetCell(vCell);

        auto MakeFace = [&](int v1, int v2, int v3, int v4, olc::Face f) {
            render.push_back({projCube[v1], projCube[v2], projCube[v3], projCube[v4], cell.id[f]});
        };

        if (!cell.wall) {
            if (bVisible[olc::Face::Floor]) MakeFace(4, 0, 1, 5, olc::Face::Floor);
        } else {
            if (bVisible[olc::Face::South]) MakeFace(3, 0, 1, 2, olc::Face::South);
            if (bVisible[olc::Face::North]) MakeFace(6, 5, 4, 7, olc::Face::North);
            if (bVisible[olc::Face::East]) MakeFace(7, 4, 0, 3, olc::Face::East);
            if (bVisible[olc::Face::West]) MakeFace(2, 1, 5, 6, olc::Face::West);
            if (bVisible[olc::Face::Top]) MakeFace(7, 3, 2, 6, olc::Face::Top);
        }
    }

    void resolveCollision(olc::vf2d &vfPos, const olc::vf2d &vfVel, float elapsedTime) {
        // The first element of vCollisionRects is the rect of the current player
        vCollisionRects[0].pos = vfPos;
        vCollisionRects[0].size = {1, 1};
        vCollisionRects[0].vel = vfVel;

        // Update vCollisionRects according to current pos of char
        olc::vi2d viPos = vfPos;
        for (int i = 1; i < vCollisionRects.size(); i++) {
            vCollisionRects[i].pos = viPos + surroundRect[i - 1];
            vCollisionRects[i].size = {1, 1};
        }

        // Sort collisions in order of distance
        olc::vf2d cp, cn;
        float t = 0;
        std::vector<std::pair<int, float>> z;

        // Work out collision point, add it to vector along with rect ID
        for (size_t i = 1; i < vCollisionRects.size(); i++) {
            if (world.GetCell(vCollisionRects[i].pos).wall)
                if (olc::aabb::DynamicRectVsRect(&vCollisionRects[0], elapsedTime, vCollisionRects[i], cp,
                                                 cn, t)) {
                    z.emplace_back(i, t);
                }
        }

        // Do the sort
        std::sort(z.begin(), z.end(), [](const std::pair<int, float> &a, const std::pair<int, float> &b) {
            return a.second < b.second;
        });

        // Now resolve the collision in correct order
        for (auto j : z)
            olc::aabb::ResolveDynamicRectVsRect(&vCollisionRects[0], elapsedTime,
                                                &vCollisionRects[j.first]);

        vCollisionRects[0].pos += vCollisionRects[0].vel * elapsedTime;
        vfPos = vCollisionRects[0].pos;
    }

    void LoadMap() {
        for (int y = 0; y < world.size.y; y++)
            for (int x = 0; x < world.size.x; x++) {
                world.GetCell({x, y}).wall = false;
                world.GetCell({x, y}).id[olc::Face::Floor] = olc::vi2d{3, 1} * vTileSize;
                world.GetCell({x, y}).id[olc::Face::Top] = olc::vi2d{0, 0} * vTileSize;
                world.GetCell({x, y}).id[olc::Face::North] = olc::vi2d{0, 0} * vTileSize;
                world.GetCell({x, y}).id[olc::Face::South] = olc::vi2d{0, 0} * vTileSize;
                world.GetCell({x, y}).id[olc::Face::West] = olc::vi2d{0, 0} * vTileSize;
                world.GetCell({x, y}).id[olc::Face::East] = olc::vi2d{0, 0} * vTileSize;
            }

        std::cout << "LOADING MAP...\n";
        std::ifstream file(mapJsonPath);
        file >> mapDict;
        int32_t key_, x, y;
        for (auto&[key, value]: mapDict.items()) {
            if (!strcmp(key.c_str(), "mapSize")) continue;
            key_ = std::stoi(key, nullptr, 10);
            x = key_ / mapSize.x;
            y = key_ - x * mapSize.x;
            if (value.contains("type")) {
                world.GetCell({x, y}).wall = value["type"] == 1;
            } else {
                world.GetCell({x, y}).wall = false; // default to floor
            }
            for (int faceid = 0; faceid < 6; faceid++) {
                if (value.contains(olc::Face_s[faceid]))
                    world.GetCell({x, y}).id[faceid] = {value[olc::Face_s[faceid]][0],
                                                        value[olc::Face_s[faceid]][1]};
            }
        }
    }

    void RenderSplash() {
        uint8_t layer_id = static_cast<uint8_t>(RenderLayer::Splash);
        SetDrawTarget(layer_id);
        Clear(olc::VERY_DARK_BLUE);

        DrawStringDecal({20.0f, 20.0f}, "Loading...", olc::YELLOW, {1.0f, 1.0f});

        EnableLayer(layer_id, true);
        EnableClearVecDecal(layer_id, false);
        SetDrawTarget(nullptr);
    }

    void RenderOpeningBg() {
        uint8_t layer_id = static_cast<uint8_t>(RenderLayer::OpeningBg);
        SetDrawTarget(layer_id);
        Clear(olc::BLANK);

        DrawStringDecal({20.0f, 20.0f}, "Start Local Game", olc::YELLOW, {1.0f, 1.0f});
        DrawStringDecal({20.0f, 30.0f}, "Join Hosted Game", olc::YELLOW, {1.0f, 1.0f});

        EnableLayer(layer_id, true);
        EnableClearVecDecal(layer_id, false);
        SetDrawTarget(nullptr);
    }

    void RenderOpeningFg() {
        uint8_t layer_id = static_cast<uint8_t>(RenderLayer::OpeningFg);
        SetDrawTarget(layer_id);
        Clear(olc::VERY_DARK_GREEN);

        DrawStringDecal({20.0f, 40.0f}, "111", olc::YELLOW, {1.0f, 1.0f});
        DrawStringDecal({20.0f, 50.0f}, "222", olc::YELLOW, {1.0f, 1.0f});

        EnableLayer(layer_id, true);
        EnableClearVecDecal(layer_id, false);
        SetDrawTarget(nullptr);
    }

    void RenderGameFg() {
        uint8_t layer_id = static_cast<uint8_t>(RenderLayer::GameFg);
        SetDrawTarget(layer_id);
        // Grab mouse for convenience
        olc::vi2d vMouse = {GetMouseX(), GetMouseY()};
        float fElapsedTime = GetElapsedTime();

        // Smooth camera
        fCameraAngle += (fCameraAngleTarget - fCameraAngle) * 10.0f * fElapsedTime;

        if (GetKey(olc::Key::W).bHeld && !GetKey(olc::Key::S).bHeld) {
            vCharVel.y = -10.0f;
        } else if (!GetKey(olc::Key::W).bHeld && GetKey(olc::Key::S).bHeld) {
            vCharVel.y = 10.0f;
        } else {
            vCharVel.y = 0;
        }

        if (GetKey(olc::Key::A).bHeld && !GetKey(olc::Key::D).bHeld) {
            vCharVel.x = -10.0f;
        } else if (!GetKey(olc::Key::A).bHeld && GetKey(olc::Key::D).bHeld) {
            vCharVel.x = 10.0f;
        } else {
            vCharVel.x = 0;
        }

        // Resolve
        resolveCollision(vCharPos, vCharVel, fElapsedTime);

        if (vCharPos.x < 0) vCharPos.x = 0;
        if (vCharPos.y < 0) vCharPos.y = 0;
        if (vCharPos.x >= world.size.x) vCharPos.x = world.size.x - 1;
        if (vCharPos.y >= world.size.y) vCharPos.y = world.size.y - 1;


        // Position camera in world
        vCameraPos = vCharPos;
        vCameraPos *= fCameraZoom;

        // Rendering

        // 1) Create dummy cube to extract visible face information
        // Cull faces that cannot be seen
        std::array<olc::vec3d, 8> cullCube = CreateCube({0, 0}, fCameraAngle, fCameraPitch, fCameraZoom,
                                                        {vCameraPos.x, 0.0f, vCameraPos.y});
        CalculateVisibleFaces(cullCube);

        // 2) Get all visible sides of all visible "tile cubes"o
        std::vector<olc::sQuad> vQuads;
        for (int y = 0; y < world.size.y; y++)
            for (int x = 0; x < world.size.x; x++)
                GetFaceQuads({x + 0.0f, y + 0.0f}, fCameraAngle, fCameraPitch, fCameraZoom,
                             {vCameraPos.x, 0.0f, vCameraPos.y},
                             vQuads);

        // 3) Sort in order of depth, from farthest away to closest
        std::sort(vQuads.begin(), vQuads.end(), [](const olc::sQuad &q1, const olc::sQuad &q2) {
            float z1 = (q1.points[0].z + q1.points[1].z + q1.points[2].z + q1.points[3].z) * 0.25f;
            float z2 = (q2.points[0].z + q2.points[1].z + q2.points[2].z + q2.points[3].z) * 0.25f;
            return z1 < z2;
        });

        // 4) Iterate through all "tile cubes" and draw their visible faces
        Clear(olc::BLACK);
        for (auto &q : vQuads)
            DrawPartialWarpedDecal
                    (
                            rendAllWalls.Decal(),
                            {{q.points[0].x, q.points[0].y},
                             {q.points[1].x, q.points[1].y},
                             {q.points[2].x, q.points[2].y},
                             {q.points[3].x, q.points[3].y}},
                            q.tile,
                            vTileSize
                    );

        // 6) Draw selection "tile cube"
        vQuads.clear();
        GetFaceQuads(vCharPos, fCameraAngle, fCameraPitch, fCameraZoom, {vCameraPos.x, 0.0f, vCameraPos.y}, vQuads);
        for (auto &q : vQuads)
            DrawWarpedDecal(rendSelect.Decal(), {{q.points[0].x, q.points[0].y},
                                                 {q.points[1].x, q.points[1].y},
                                                 {q.points[2].x, q.points[2].y},
                                                 {q.points[3].x, q.points[3].y}});

        // 7) Draw some debug info
        DrawStringDecal({0, 0}, "Cursor: " + std::to_string(vCharPos.x) + ", " + std::to_string(vCharPos.y), olc::YELLOW,
                        {0.5f, 0.5f});
        DrawStringDecal({0, 8}, "Angle: " + std::to_string(fCameraAngle) + ", " + std::to_string(fCameraPitch),
                        olc::YELLOW, {0.5f, 0.5f});
        DrawStringDecal({0, 16}, mapDict.dump(2), olc::YELLOW, {0.5f, 0.5f});

        EnableLayer(layer_id, true);
        SetDrawTarget(nullptr);
    }

public:
    bool OnUserCreate() override {
        // Initialize layers:
        // 0: Splash screen
        // 1: Opening screen animation
        // 2: Opening screen buttons
        // 3: Lobby screen map
        // 4: Lobby screen button + house rule info
        // 5: Lobby screen house rule editor / character editor
        // 6: Game screen map
        // 7: Game screen actions buttons (MINIMAP/KILL/USE/...)
        // 8: Game screen chat/minimap
        // 9: Game screen vote
        for (int i = 0; i < 10; i++) {
            nLayerBackground = CreateLayer();
        }
        layerToRender = RenderLayer::Splash;

        // Load sprite sheets
        rendAllWalls.Load(spriteSheetPath);
        // Create empty world
        world.Create(mapSize.x, mapSize.y);

        SetDrawTarget(nullptr);

        return true;
    }

    bool isShowingLayer = false;

    bool OnUserUpdate(float fElapsedTime) override {
        Clear(olc::BLANK);
        switch (layerToRender) {
            case (RenderLayer::Splash): {
                if (!isShowingLayer) {
                    RenderSplash();
                    isShowingLayer = true;
                } else {
                    LoadMap();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    EnableLayer(static_cast<uint8_t>(RenderLayer::Splash), false);
                    layerToRender = RenderLayer::OpeningBg;
                    isShowingLayer = false;
                }
                break;
            }
            case RenderLayer::OpeningBg: {
                if (!isShowingLayer) {
                    RenderOpeningBg();
                    isShowingLayer = true;
                } else {
                    layerToRender = RenderLayer::OpeningFg;
                    isShowingLayer = false;
                }
                break;
            }
            case RenderLayer::OpeningFg: {
                if (!isShowingLayer) {
                    RenderOpeningFg();
                    isShowingLayer = true;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // simulate process
                    EnableLayer(static_cast<uint8_t>(RenderLayer::OpeningBg), false);
                    EnableLayer(static_cast<uint8_t>(RenderLayer::OpeningFg), false);
                    layerToRender = RenderLayer::GameFg;
                    isShowingLayer = false;
                }
                break;
            }
            case RenderLayer::LobbyMap:
                break;
            case RenderLayer::LobbyBg:
                break;
            case RenderLayer::LobbyFg:
                break;
            case RenderLayer::GameMap:
                break;
            case RenderLayer::GameBg:
                break;
            case RenderLayer::GameFg: {
                RenderGameFg();
                break;
            }
        }

        // Graceful exit if user is in full screen mode
        return !GetKey(olc::Key::ESCAPE).bPressed;
    }

protected:
    void handleNetworking() {
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
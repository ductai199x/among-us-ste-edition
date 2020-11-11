#define OLC_PGE_APPLICATION
#include "helper/json.hpp"
#include "helper/olcPixelGameEngine.h"

using chrono_clock = std::chrono::steady_clock;
using json = nlohmann::json;

namespace olc {
    struct vec3d { float x, y, z; };

	struct sQuad
	{
		vec3d points[4];
		olc::vf2d tile;
	};

	struct sCell
	{
		bool wall = false;
		olc::vi2d id[6]{};
	};

    class World
	{
	public:
		World() {}

		void Create(int w, int h)
		{
			size = { w, h };
			vCells.resize(w * h);
		}

		sCell& GetCell(const olc::vi2d& v)
		{
			if (v.x >= 0 && v.x < size.x && v.y >= 0 && v.y < size.y)
				return vCells[v.y * size.x + v.x];
			else
				return NullCell;
		}

	public:
		olc::vi2d size;

	private:
		std::vector<sCell> vCells;
		sCell NullCell;
	};

    enum Face
	{
		Floor = 0,
		North = 1,
		East = 2,
		South = 3,
		West = 4,
		Top = 5
	};

	std::vector<std::string> Face_s = {"0", "1", "2", "3", "4", "5"};
}

class MapDesigner : public olc::PixelGameEngine {
public:
    MapDesigner() {
        sAppName = "Among Us STE Map Designer";
    }

protected:
    std::string assetsPath = "/home/bigboy/1-workdir/among-us-ste-edition/assets/";
    std::string spriteSheetPath = assetsPath + "32x32.png";
    std::string mapJsonPath = assetsPath + "map.json";

	json mapDict;
    olc::World world;
	olc::Renderable rendSelect;
	olc::Renderable rendAllWalls;

	olc::vf2d vCameraPos = { 0.0f, 0.0f };
	float fCameraAngle = 0.0f;
	float fCameraAngleTarget = fCameraAngle;
	float fCameraPitch = 5.5f;
	float fCameraZoom = 16.0f;

	bool bVisible[6];

	olc::vi2d vCursor = { 0, 0 };
	olc::vi2d vTileCursor = { 0,0 };
	olc::vi2d vTileSize = { 32, 32 };
    olc::vi2d mapSize = {64, 64};

	std::string cursorId;

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
	bool OnUserCreate() override {
        rendAllWalls.Load(spriteSheetPath);
		world.Create(mapSize.x, mapSize.y);

        mapDict["mapSize"] = { mapSize.x, mapSize.y };

        for (int y = 0; y < world.size.y; y++)
			for (int x = 0; x < world.size.x; x++)
			{
				world.GetCell({ x, y }).wall = false;
				world.GetCell({ x, y }).id[olc::Face::Floor] = olc::vi2d{ 3, 1 } *vTileSize;
				world.GetCell({ x, y }).id[olc::Face::Top] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[olc::Face::North] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[olc::Face::South] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[olc::Face::West] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[olc::Face::East] = olc::vi2d{ 0, 0 } *vTileSize;
			}

        return true;
    }

    bool OnUserUpdate(float fElapsedTime) override {
        // called once per frame
        // Grab mouse for convenience
		olc::vi2d vMouse = { GetMouseX(), GetMouseY() };

		// Translate cursor position to string id:
		cursorId = std::to_string(vCursor.y + vCursor.x*mapSize.x); 

		// Edit mode - Selection from tile sprite sheet
		if (GetKey(olc::Key::TAB).bHeld)
		{
			DrawSprite({ 0, 0 }, rendAllWalls.Sprite());
			DrawRect(vTileCursor * vTileSize, vTileSize);
			if (GetMouse(0).bPressed) vTileCursor = vMouse / vTileSize;
			return true;
		}

		// WS keys to tilt camera
		if (GetKey(olc::Key::W).bHeld) fCameraPitch += 1.0f * fElapsedTime;
		if (!GetKey(olc::Key::CTRL).bHeld && GetKey(olc::Key::S).bHeld) fCameraPitch -= 1.0f * fElapsedTime;

		// DA Keys to manually rotate camera
		if (GetKey(olc::Key::D).bHeld) fCameraAngleTarget += 1.0f * fElapsedTime;
		if (GetKey(olc::Key::A).bHeld) fCameraAngleTarget -= 1.0f * fElapsedTime;

		// QZ Keys to zoom in or out
		if (GetKey(olc::Key::Q).bHeld) fCameraZoom += 5.0f * fElapsedTime;
		if (GetKey(olc::Key::Z).bHeld) fCameraZoom -= 5.0f * fElapsedTime;

		// Numpad keys used to rotate camera to fixed angles
		if (GetKey(olc::Key::NP2).bPressed) fCameraAngleTarget = 3.14159f * 0.0f;
		if (GetKey(olc::Key::NP1).bPressed) fCameraAngleTarget = 3.14159f * 0.25f;
		if (GetKey(olc::Key::NP4).bPressed) fCameraAngleTarget = 3.14159f * 0.5f;
		if (GetKey(olc::Key::NP7).bPressed) fCameraAngleTarget = 3.14159f * 0.75f;
		if (GetKey(olc::Key::NP8).bPressed) fCameraAngleTarget = 3.14159f * 1.0f;
		if (GetKey(olc::Key::NP9).bPressed) fCameraAngleTarget = 3.14159f * 1.25f;
		if (GetKey(olc::Key::NP6).bPressed) fCameraAngleTarget = 3.14159f * 1.5f;
		if (GetKey(olc::Key::NP3).bPressed) fCameraAngleTarget = 3.14159f * 1.75f;

		// Numeric keys apply selected tile to specific face
		olc::vi2d vSpriteTileSelect = vTileCursor * vTileSize;
		if (GetKey(olc::Key::K1).bPressed) {
			world.GetCell(vCursor).id[olc::Face::North] = vSpriteTileSelect;
			mapDict[cursorId][olc::Face_s[olc::Face::North]] = {vSpriteTileSelect.x, vSpriteTileSelect.y};
		}
		if (GetKey(olc::Key::K2).bPressed) {
			world.GetCell(vCursor).id[olc::Face::East] = vSpriteTileSelect;
			mapDict[cursorId][olc::Face_s[olc::Face::East]] = {vSpriteTileSelect.x, vSpriteTileSelect.y};
		}
		if (GetKey(olc::Key::K3).bPressed) {
			world.GetCell(vCursor).id[olc::Face::South] = vSpriteTileSelect;
			mapDict[cursorId][olc::Face_s[olc::Face::South]] = {vSpriteTileSelect.x, vSpriteTileSelect.y};
		}
		if (GetKey(olc::Key::K4).bPressed) {
			world.GetCell(vCursor).id[olc::Face::West] = vSpriteTileSelect;
			mapDict[cursorId][olc::Face_s[olc::Face::West]] = {vSpriteTileSelect.x, vSpriteTileSelect.y};
		}
		if (GetKey(olc::Key::K5).bPressed) {
			world.GetCell(vCursor).id[olc::Face::Floor] = vSpriteTileSelect;
			mapDict[cursorId][olc::Face_s[olc::Face::Floor]] = {vSpriteTileSelect.x, vSpriteTileSelect.y};
		}
		if (GetKey(olc::Key::K6).bPressed) {
			world.GetCell(vCursor).id[olc::Face::Top] = vSpriteTileSelect;
			mapDict[cursorId][olc::Face_s[olc::Face::Top]] = {vSpriteTileSelect.x, vSpriteTileSelect.y};
		}

		// Smooth camera
		fCameraAngle += (fCameraAngleTarget - fCameraAngle) * 10.0f * fElapsedTime;

		// Arrow keys to move the selection cursor around map (boundary checked)
		if (GetKey(olc::Key::LEFT).bPressed) vCursor.x--;
		if (GetKey(olc::Key::RIGHT).bPressed) vCursor.x++;
		if (GetKey(olc::Key::UP).bPressed) vCursor.y--;
		if (GetKey(olc::Key::DOWN).bPressed) vCursor.y++;
		if (vCursor.x < 0) vCursor.x = 0;
		if (vCursor.y < 0) vCursor.y = 0;
		if (vCursor.x >= world.size.x) vCursor.x = world.size.x - 1;
		if (vCursor.y >= world.size.y) vCursor.y = world.size.y - 1;

		// Place block with space
		if (GetKey(olc::Key::SPACE).bPressed)
		{
			world.GetCell(vCursor).wall = !world.GetCell(vCursor).wall;
			if (world.GetCell(vCursor).wall) {
				mapDict[cursorId]["type"] = 1;
				mapDict[cursorId][olc::Face_s[olc::Face::Floor]] = {0, 0};
				mapDict[cursorId][olc::Face_s[olc::Face::North]] = {0, 0};
				mapDict[cursorId][olc::Face_s[olc::Face::East]] = {0, 0};
				mapDict[cursorId][olc::Face_s[olc::Face::South]] = {0, 0};
				mapDict[cursorId][olc::Face_s[olc::Face::West]] = {0, 0};
				mapDict[cursorId][olc::Face_s[olc::Face::Top]] = {0, 0};
			} else {
				mapDict.erase(cursorId);
			}
		}

        // Position camera in world		
		vCameraPos = { vCursor.x + 0.5f, vCursor.y + 0.5f };
		vCameraPos *= fCameraZoom;

		// Rendering

		// 1) Create dummy cube to extract visible face information
		// Cull faces that cannot be seen
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

		// 5) Draw current tile selection
		DrawPartialDecal({ 10,10 }, rendAllWalls.Decal(), vTileCursor * vTileSize, vTileSize);

		// 6) Draw selection "tile cube"
		vQuads.clear();
		GetFaceQuads(vCursor, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y }, vQuads);
		for (auto& q : vQuads)
			DrawWarpedDecal(rendSelect.Decal(), { {q.points[0].x, q.points[0].y}, {q.points[1].x, q.points[1].y}, {q.points[2].x, q.points[2].y}, {q.points[3].x, q.points[3].y} });

		// 7) Draw some debug info
		DrawStringDecal({ 0,0 }, "Cursor: " + std::to_string(vCursor.x) + ", " + std::to_string(vCursor.y), olc::YELLOW, { 0.5f, 0.5f });
		DrawStringDecal({ 0,8 }, "Angle: " + std::to_string(fCameraAngle) + ", " + std::to_string(fCameraPitch), olc::YELLOW, { 0.5f, 0.5f });
		DrawStringDecal({ 0,16 }, mapDict.dump(2), olc::YELLOW, { 0.5f, 0.5f });

		// 8) Save the map onto the drive
		if (GetKey(olc::Key::CTRL).bHeld && GetKey(olc::Key::C).bPressed) {
			std::cout << "SAVING MAP...\n";
			std::ofstream o(mapJsonPath);
			o << std::setw(4) << mapDict << std::endl;
		}

		if (GetKey(olc::Key::CTRL).bHeld && GetKey(olc::Key::O).bPressed) {
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
		}

		// Graceful exit if user is in full screen mode
		return !GetKey(olc::Key::ESCAPE).bPressed;
    }
};

int main() {
    MapDesigner game;
    if (game.Construct(640, 480, 2, 2, false))
        game.Start();
    return 0;
}

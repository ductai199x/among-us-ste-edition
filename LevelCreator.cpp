﻿#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <string>

// For convenience 
using json = nlohmann::json;
namespace fs = std::experimental::filesystem;
using namespace std;

class olcDungeon : public olc::PixelGameEngine
{
public:
	olcDungeon()
	{
		sAppName = "Dungeon Explorer";
	}

	struct Renderable
	{
		Renderable() {}

		void Load(const std::string& sFile)
		{
			sprite = new olc::Sprite(sFile);
			decal = new olc::Decal(sprite);
		}

		~Renderable()
		{
			delete decal;
			delete sprite;
		}

		olc::Sprite* sprite = nullptr;
		olc::Decal* decal = nullptr;
	};

	struct vec3d
	{
		float x, y, z;
	};

	struct sQuad
	{
		vec3d points[4];
		olc::vf2d tile;
	};

	struct sCell
	{
		bool wall = false;
		olc::vi2d id[6]{  };
	};

	class World
	{
	public:
		World()
		{

		}

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

	World world;
	Renderable rendSelect;
	Renderable rendAllWalls;

	olc::vf2d vCameraPos = { 0.0f, 0.0f };
	float fCameraAngle = 0.0f;
	float fCameraAngleTarget = fCameraAngle;
	float fCameraPitch = 5.5f;
	float fCameraZoom = 16.0f;

	bool bVisible[6];

	olc::vi2d vCursor = { 0, 0 };
	olc::vi2d vTileCursor = { 0,0 };
	olc::vi2d vTileSize = { 32, 32 };

	enum Face
	{
		Floor = 0,
		North = 1,
		East = 2,
		South = 3,
		West = 4,
		Top = 5
	};
	string modeSelect;
public:
	bool OnUserCreate() override
	{
		rendSelect.Load("./gfx/dng_select.png");
		rendAllWalls.Load("./gfx/oldDungeon.png");
		world.Create(64, 64);

		// Preload tiles
		for (int y = 0; y < world.size.y; y++)
			for (int x = 0; x < world.size.x; x++)
			{
				world.GetCell({ x, y }).wall = false;
				world.GetCell({ x, y }).id[Face::Floor] = olc::vi2d{ 3, 1 } *vTileSize;
				world.GetCell({ x, y }).id[Face::Top] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[Face::North] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[Face::South] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[Face::West] = olc::vi2d{ 0, 0 } *vTileSize;
				world.GetCell({ x, y }).id[Face::East] = olc::vi2d{ 0, 0 } *vTileSize;
			}

		// Ask user for mode selection 
		bool promptState = true;
		while (promptState) {
			cout << "Choose your mode (Design(d) or Load(l)): ";
			cin >> modeSelect;
			if (modeSelect == "d") 
			{
				std::cout << "Design mode selected"<<endl;
				promptState = false;
			}
			else if (modeSelect == "l") {
				// path variables
				string dir = "./maps/";
				string extension = ".json";
				string map = "map";
				int numbMap;
				cout << "Choose the map you want to load: ";
				cin >> numbMap;
				string pathname = dir + map + to_string(numbMap) + extension;

				// load json file
				std::ifstream file(pathname, std::ifstream::binary);
				json jsonfile;
				file >> jsonfile;
				for (auto& element : jsonfile) {
					world.GetCell({ element["id"][0], element["id"][1] }).wall = true;
				}

				// Update Player Position 
				vCameraPos = { 32.0f, 32.0f };
				fCameraPitch = 4.5f;
				fCameraZoom = 25.0f;
				vCursor = { 10, 10 };

				// Render Player 
				rendSelect.Load("./gfx/character.png");

				// Output
				std::cout << "Load mode selected" << endl;
				promptState = false;
				
			}
			else {
				std::cout << "Invalid choice" << endl;
			}
		}
		

		
		return true;
	}

	std::array<vec3d, 8> CreateCube(const olc::vi2d& vCell, const float fAngle, const float fPitch, const float fScale, const vec3d& vCamera)
	{
		// Unit Cube
		std::array<vec3d, 8> unitCube, rotCube, worldCube, projCube;
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

	void CalculateVisibleFaces(std::array<vec3d, 8>& cube)
	{
		auto CheckNormal = [&](int v1, int v2, int v3)
		{
			olc::vf2d a = { cube[v1].x, cube[v1].y };
			olc::vf2d b = { cube[v2].x, cube[v2].y };
			olc::vf2d c = { cube[v3].x, cube[v3].y };
			return  (b - a).cross(c - a) > 0;
		};

		bVisible[Face::Floor] = CheckNormal(4, 0, 1);
		bVisible[Face::South] = CheckNormal(3, 0, 1);
		bVisible[Face::North] = CheckNormal(6, 5, 4);
		bVisible[Face::East] = CheckNormal(7, 4, 0);
		bVisible[Face::West] = CheckNormal(2, 1, 5);
		bVisible[Face::Top] = CheckNormal(7, 3, 2);
	}

	void GetFaceQuads(const olc::vi2d& vCell, const float fAngle, const float fPitch, const float fScale, const vec3d& vCamera, std::vector<sQuad>& render)
	{
		std::array<vec3d, 8> projCube = CreateCube(vCell, fAngle, fPitch, fScale, vCamera);

		auto& cell = world.GetCell(vCell);

		auto MakeFace = [&](int v1, int v2, int v3, int v4, Face f)
		{
			render.push_back({ projCube[v1], projCube[v2], projCube[v3], projCube[v4], cell.id[f] });
		};

		if (!cell.wall)
		{
			if (bVisible[Face::Floor]) MakeFace(4, 0, 1, 5, Face::Floor);
		}
		else
		{
			if (bVisible[Face::South]) MakeFace(3, 0, 1, 2, Face::South);
			if (bVisible[Face::North]) MakeFace(6, 5, 4, 7, Face::North);
			if (bVisible[Face::East]) MakeFace(7, 4, 0, 3, Face::East);
			if (bVisible[Face::West]) MakeFace(2, 1, 5, 6, Face::West);
			if (bVisible[Face::Top]) MakeFace(7, 3, 2, 6, Face::Top);
		}
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		// Grab mouse for convenience
		olc::vi2d vMouse = { GetMouseX(), GetMouseY() };

		// Edit mode - Selection from tile sprite sheet
		if (GetKey(olc::Key::TAB).bHeld)
		{
			DrawSprite({ 0, 0 }, rendAllWalls.sprite);
			DrawRect(vTileCursor * vTileSize, vTileSize);
			if (GetMouse(0).bPressed) vTileCursor = vMouse / vTileSize;
			return true;
		}

		// WS keys to tilt camera
		if (GetKey(olc::Key::W).bHeld) fCameraPitch += 1.0f * fElapsedTime;
		if (GetKey(olc::Key::S).bHeld) fCameraPitch -= 1.0f * fElapsedTime;

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
		if (GetKey(olc::Key::K1).bPressed) world.GetCell(vCursor).id[Face::North] = vTileCursor * vTileSize;
		if (GetKey(olc::Key::K2).bPressed) world.GetCell(vCursor).id[Face::East] = vTileCursor * vTileSize;
		if (GetKey(olc::Key::K3).bPressed) world.GetCell(vCursor).id[Face::South] = vTileCursor * vTileSize;
		if (GetKey(olc::Key::K4).bPressed) world.GetCell(vCursor).id[Face::West] = vTileCursor * vTileSize;
		if (GetKey(olc::Key::K5).bPressed) world.GetCell(vCursor).id[Face::Floor] = vTileCursor * vTileSize;
		if (GetKey(olc::Key::K6).bPressed) world.GetCell(vCursor).id[Face::Top] = vTileCursor * vTileSize;

		// Smooth camera
		fCameraAngle += (fCameraAngleTarget - fCameraAngle) * 10.0f * fElapsedTime;

		// Arrow keys to move the selection cursor around map (boundary checked)
		if (modeSelect == "l") {
			if (GetKey(olc::Key::LEFT).bPressed)
			{
				olc::vi2d pCursor = vCursor;
				vCursor.x--;
				if (world.GetCell(vCursor).wall == true) {
					vCursor = pCursor;
				}
			}
			if (GetKey(olc::Key::RIGHT).bPressed)
			{
				olc::vi2d pCursor = vCursor;
				vCursor.x++;
				if (world.GetCell(vCursor).wall == true) {
					vCursor = pCursor;
				}
			}
			if (GetKey(olc::Key::UP).bPressed)
			{
				olc::vi2d pCursor = vCursor;
				vCursor.y--;
				if (world.GetCell(vCursor).wall == true) {
					vCursor = pCursor;
				}
			}
			if (GetKey(olc::Key::DOWN).bPressed)
			{
				olc::vi2d pCursor = vCursor;
				vCursor.y++;
				if (world.GetCell(vCursor).wall == true) {
					vCursor = pCursor;
				}
			};
		}
		if (modeSelect == "d") {
			if (GetKey(olc::Key::LEFT).bPressed) vCursor.x--;
			if (GetKey(olc::Key::RIGHT).bPressed) vCursor.x++;
			if (GetKey(olc::Key::UP).bPressed) vCursor.y--;
			if (GetKey(olc::Key::DOWN).bPressed) vCursor.y++;
		}
		if (vCursor.x < 0) vCursor.x = 0;
		if (vCursor.y < 0) vCursor.y = 0;
		if (vCursor.x >= world.size.x) vCursor.x = world.size.x - 1;
		if (vCursor.y >= world.size.y) vCursor.y = world.size.y - 1;

		// Place block with space
		if (GetKey(olc::Key::SPACE).bPressed)
		{
			world.GetCell(vCursor).wall = !world.GetCell(vCursor).wall;
		}
		// Save Game 
		if (GetKey(olc::Key::X).bPressed) {
			// path variables
			string dir = "./maps/";
			string extension = ".json";
			string map = "map";

			// Auto generate new map 
			int numbMap = 1;
			for (auto& p : fs::directory_iterator(dir)) {
				numbMap++;
			}
			string pathname = dir + map + to_string(numbMap) + extension;

			// Open new file
			json jsonfile;
			std::ofstream file;
			file.open(pathname, std::ios_base::app);

			// Saving states of every pixels
			for (int x_cursor = 0; x_cursor <= 63; x_cursor++) {
				for (int y_cursor = 0; y_cursor <= 63; y_cursor++) {
					if (world.GetCell({ x_cursor, y_cursor }).wall == true) {
						json jsonElement;
						jsonElement["id"] = { x_cursor, y_cursor };
						jsonElement["Floor"] = { world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].x, world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].y };
						jsonElement["North"] = { world.GetCell({ x_cursor, y_cursor }).id[Face::North].x, world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].y };
						jsonElement["East"] = { world.GetCell({ x_cursor, y_cursor }).id[Face::East].x, world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].y };
						jsonElement["South"] = { world.GetCell({ x_cursor, y_cursor }).id[Face::South].x, world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].y };
						jsonElement["West"] = { world.GetCell({ x_cursor, y_cursor }).id[Face::West].x, world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].y };
						jsonElement["Top"] = { world.GetCell({ x_cursor, y_cursor }).id[Face::Top].x, world.GetCell({ x_cursor, y_cursor }).id[Face::Floor].y };

						jsonfile.push_back(jsonElement);
						
					}
				}
			}
			file << jsonfile;

			// Notify the user that the saving process is done 
			string notifyUser = map + to_string(numbMap) + "is saved";
			DrawStringDecal({ 32,32 }, notifyUser, olc::YELLOW, { 8.0f, 8.f });
		}

		// Position camera in world		
		vCameraPos = { vCursor.x + 0.5f, vCursor.y + 0.5f };
		vCameraPos *= fCameraZoom;

		// Rendering

		// 1) Create dummy cube to extract visible face information
		// Cull faces that cannot be seen
		std::array<vec3d, 8> cullCube = CreateCube({ 0, 0 }, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y });
		CalculateVisibleFaces(cullCube);

		// 2) Get all visible sides of all visible "tile cubes"o
		std::vector<sQuad> vQuads;
		for (int y = 0; y < world.size.y; y++)
			for (int x = 0; x < world.size.x; x++)
				GetFaceQuads({ x, y }, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y }, vQuads);

		// 3) Sort in order of depth, from farthest away to closest
		std::sort(vQuads.begin(), vQuads.end(), [](const sQuad& q1, const sQuad& q2)
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
				rendAllWalls.decal,
				{ {q.points[0].x, q.points[0].y}, {q.points[1].x, q.points[1].y}, {q.points[2].x, q.points[2].y}, {q.points[3].x, q.points[3].y} },
				q.tile,
				vTileSize
			);

		// 5) Draw current tile selection
		DrawPartialDecal({ 10,10 }, rendAllWalls.decal, vTileCursor * vTileSize, vTileSize);

		// 6) Draw selection "tile cube"	
		vQuads.clear();
		GetFaceQuads(vCursor, fCameraAngle, fCameraPitch, fCameraZoom, { vCameraPos.x, 0.0f, vCameraPos.y }, vQuads);
		for (auto& q : vQuads)
			DrawWarpedDecal(rendSelect.decal, { {q.points[0].x, q.points[0].y}, {q.points[1].x, q.points[1].y}, {q.points[2].x, q.points[2].y}, {q.points[3].x, q.points[3].y} });

		// 7) Draw some debug info
		DrawStringDecal({ 0,0 }, "Cursor: " + std::to_string(vCursor.x) + ", " + std::to_string(vCursor.y), olc::YELLOW, { 0.5f, 0.5f });
		DrawStringDecal({ 0,8 }, "Angle: " + std::to_string(fCameraAngle) + ", " + std::to_string(fCameraPitch), olc::YELLOW, { 0.5f, 0.5f });
		DrawStringDecal({ 0,16 }, "Press X to save current map", olc::YELLOW, { 0.5f, 0.5f });
		// Graceful exit if user is in full screen mode
		return !GetKey(olc::Key::ESCAPE).bPressed;
	}
};

int main()
{
	olcDungeon demo;
	if (demo.Construct(640, 480, 2, 2, false))
		demo.Start();
	return 0;
}
//
// Created by bigboy on 11/10/20.
//

#ifndef AMONG_US_STE_EDITION_OLCEXTENDED_H
#define AMONG_US_STE_EDITION_OLCEXTENDED_H

#include "olcPixelGameEngine.h"

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

#endif //AMONG_US_STE_EDITION_OLCEXTENDED_H

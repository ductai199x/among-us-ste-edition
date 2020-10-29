//
// Created by bigboy on 10/28/20.
//

#ifndef AMONG_US_STE_EDITION_GAMEINSTANCE_H
#define AMONG_US_STE_EDITION_GAMEINSTANCE_H

#include <iomanip>
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

class GameInstance {
public:
    GameInstance();

    ~GameInstance();

    bool UpdateDatabase(json &updates);

    void PrintDatabase();

private:
    json database;
};


#endif //AMONG_US_STE_EDITION_GAMEINSTANCE_H

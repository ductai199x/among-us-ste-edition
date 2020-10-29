
#include "GameInstance.h"

GameInstance::GameInstance() {
    database["game_state1"] = 1;
    database["game_state2"] = 2.5;
    database["game_state3"] = {1, 2, 3, 4};
}

GameInstance::~GameInstance() {
    database.clear();
}

bool GameInstance::UpdateDatabase(json &updates) {
    for (auto &[key, value] : updates.items()) {
        std::cout << "Updating: " << key << " : " << value << std::endl;
        if (database.contains(key)) {
            database[key] = value;
        } else {
            std::cout << key << " does not exist in database" << std::endl;
        }
    }
    return true;
}

void GameInstance::PrintDatabase() {
    std::cout << std::setw(4) << database << std::endl;
}
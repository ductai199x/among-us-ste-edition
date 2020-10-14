#include "RPG_Engine.h"

int main()
{
	RPG_Engine game;
	if (game.ConstructConsole(256, 240, 2, 2))
		game.Start();
	return 0;
}
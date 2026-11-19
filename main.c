#include "headers/std.h"
#include "raylib.h" 
#include "headers/Game.h"
#include "scenes/api/EndStartLoader.h"
#include "scenes/menu/EnterToPlay.h"
 
int main() { 
	RayGame_Start("Friday Night Funkin' Cengine");
	AllScenes_StartGame();
	EnterToPlay_SetScene(); 

	RayGame_GameLoop();
	AllScenes_DestroyGame();
	RayGame_Destroy();

	return 0;
}

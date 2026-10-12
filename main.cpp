#include "game.h"

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return Game::run();
}
#else
int main()
{
    return Game::run();
}
#endif

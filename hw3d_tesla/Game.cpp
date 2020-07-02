#include "Game.h"
#include "imgui/imgui.h"

Game::Game()
	:
	wnd(Graphics::ScreenWidth, Graphics::ScreenHeight, "Adrian Tesla DirectX Framework", 200, 200),
	gfx(wnd.GetHwnd())
{
}

void Game::Go()
{
	gfx.BeginFrame();
	UpdateModel();
	ComposeFrame();
	gfx.EndFrame();
}

void Game::UpdateModel()
{
}

void Game::ComposeFrame()
{
	gfx.PutPixel(400, 300, Color::Azure);
}
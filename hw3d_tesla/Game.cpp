#include "Game.h"
#include "imgui/imgui.h"

Game::Game()
	:
	wnd(Graphics::ScreenWidth, Graphics::ScreenHeight, "Adrian Tesla DirectX Framework", 250, 250),
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
}
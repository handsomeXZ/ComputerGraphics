#include "iostream"
#include "SDL.h"
#include "algorithm"
#include "cstdlib"
#include "Pixel.h"

using namespace std;



typedef struct Vertex {
	float x;
	float y;
	float r;
	float g;
	float b;
	Vertex(float x,float y,float r,float g,float b) {
		this->x = x;
		this->y = y;
		this->r = r;
		this->g = g;
		this->b = b;
	}
};
typedef struct BarycentricCoordinates
{
	float ratio;

	float Evaluate(Vertex v0,Vertex v1, Vertex v2,float x,float y) {
		this->ratio = (-1 * (x - v1.x) * (v2.y - v1.y) + (y - v1.y) * (v2.x - v1.x)) /
			(-1 * (v0.x - v1.x) * (v2.y - v1.y) + (v0.y - v1.y) * (v2.x - v1.x));
		return this->ratio;
	}

};

void drawTriangle2(const Vertex& v0, const Vertex& v1, const Vertex& v2, SDL_Surface* m_surface, SDL_Window* m_window);


int main2(int argc, char* argv[])
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* window = SDL_CreateWindow(
		"SDL2BarycentricEvaluate",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		640,
		480,
		0
	);

	SDL_Surface* screen = SDL_GetWindowSurface(window);
	SDL_FillRect(screen, 0, 0);
	Vertex v0(100, 100, 1, 0, 0);
	Vertex v1(500, 100, 0, 1, 0);
	Vertex v2(300, 400, 0, 0, 1);

	drawTriangle2(v0, v1, v2, screen, window);


	system("pause");
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}


void drawTriangle2(const Vertex& v0, const Vertex& v1, const Vertex& v2, SDL_Surface* m_surface, SDL_Window* m_window)
{
	int minX = std::min(std::min(v0.x, v1.x), v2.x);
	int maxX = std::max(std::max(v0.x, v1.x), v2.x);
	int minY = std::min(std::min(v0.y, v1.y), v2.y);
	int maxY = std::max(std::max(v0.y, v1.y), v2.y);

	BarycentricCoordinates* a = new BarycentricCoordinates;
	BarycentricCoordinates* b = new BarycentricCoordinates;
	BarycentricCoordinates* c = new BarycentricCoordinates;
	Pixel* pixel = new Pixel();


	// Add 0.5 to sample at pixel centers.
	for (float x = minX + 0.5f, xm = maxX + 0.5f; x <= xm; x += 1.0f)
		for (float y = minY + 0.5f, ym = maxY + 0.5f; y <= ym; y += 1.0f)
		{
			if (a->Evaluate(v0,v1,v2,x,y)>=0 && b->Evaluate(v1, v2, v0, x, y) >= 0 && c->Evaluate(v2, v0, v1, x, y) >= 0)
			{
				int rint =  (a->ratio * v0.r + b->ratio * v1.r + c->ratio * v2.r) * 255;
				int gint =  (a->ratio * v0.g + b->ratio * v1.g + c->ratio * v2.g) * 255;
				int bint =  (a->ratio * v0.b + b->ratio * v1.b + c->ratio * v2.b) * 255;
				Uint32 color = SDL_MapRGB(m_surface->format, rint, gint, bint);
				/*cout << "x:" << x << "  y:" << y << "  color:"<< c->ratio << endl;*/
				pixel->putpixel(m_surface, x, y, color);

			}
		}
	SDL_UpdateWindowSurface(m_window);
	cout << "Íê³ÉäÖÈ¾" << endl;
}

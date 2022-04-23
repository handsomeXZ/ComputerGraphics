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
	Vertex(float x,
		float y,
		float r,
		float g,
		float b) {
		this->x = x;
		this->y = y;
		this->r = r;
		this->g = g;
		this->b = b;
	}
};
void drawTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, SDL_Surface* m_surface, SDL_Window* m_window);



int main(int argc, char* argv[])
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* window = SDL_CreateWindow(
		"SDL2EdgeEvaluate",
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

	drawTriangle(v0,v1,v2,screen,window);


	system("pause");
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}


typedef struct EdgeEquation {
	float a;
	float b;
	float c;
	bool tie;

	EdgeEquation(const Vertex& v0, const Vertex& v1)
	{
		a = v0.y - v1.y;
		b = v1.x - v0.x;
		c = -(a * (v0.x + v1.x) + b * (v0.y + v1.y)) / 2;
		tie = a != 0 ? a > 0 : b > 0;
	}

	/// Evaluate the edge equation for the given point.
	float evaluate(float x, float y)
	{
		return a * x + b * y + c;
	}

	/// Test if the given point is inside the edge.
	bool test(float x, float y)
	{
		return test(evaluate(x, y));
	}

	/// Test for a given evaluated value.
	bool test(float v)
	{
		return (v > 0 || v == 0 && tie);
	}
};

typedef struct ParameterEquation {
	float a;
	float b;
	float c;

	ParameterEquation(
		float p0,
		float p1,
		float p2,
		const EdgeEquation& e0,
		const EdgeEquation& e1,
		const EdgeEquation& e2,
		float area)
	{
		float factor = 1.0f / (2.0f * area);

		a = factor * (p0 * e0.a + p1 * e1.a + p2 * e2.a);
		b = factor * (p0 * e0.b + p1 * e1.b + p2 * e2.b);
		c = factor * (p0 * e0.c + p1 * e1.c + p2 * e2.c);
	}

	/// Evaluate the parameter equation for the given point.
	float evaluate(float x, float y)
	{
		return a * x + b * y + c;
	}
};
void drawTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,SDL_Surface* m_surface,SDL_Window* m_window)
{
	// Compute triangle bounding box.
	int minX = std::min(std::min(v0.x, v1.x), v2.x);
	int maxX = std::max(std::max(v0.x, v1.x), v2.x);
	int minY = std::min(std::min(v0.y, v1.y), v2.y);
	int maxY = std::max(std::max(v0.y, v1.y), v2.y);

	// Clip to scissor rect.
// 	minX = std::max(minX, m_minX);
// 	maxX = std::min(maxX, m_maxX);
// 	minY = std::max(minY, m_minY);
// 	maxY = std::min(maxY, m_maxY);

	// Compute edge equations.
	EdgeEquation e0(v1, v2);
	EdgeEquation e1(v2, v0);
	EdgeEquation e2(v0, v1);

	float area = 0.5 * (e0.c + e1.c + e2.c);

	if (area < 0) {
		cout << "Quit" << endl;
		return;
	}

		

	ParameterEquation r(v0.r, v1.r, v2.r, e0, e1, e2, area);
	ParameterEquation g(v0.g, v1.g, v2.g, e0, e1, e2, area);
	ParameterEquation b(v0.b, v1.b, v2.b, e0, e1, e2, area);
	Pixel* pixel = new Pixel();

	// Add 0.5 to sample at pixel centers.
	for (float x = minX + 0.5f, xm = maxX + 0.5f; x <= xm; x += 1.0f)
		for (float y = minY + 0.5f, ym = maxY + 0.5f; y <= ym; y += 1.0f)
		{
			if (e0.test(x, y) && e1.test(x, y) && e2.test(x, y))
			{
				int rint = r.evaluate(x, y) * 255;
				int gint = g.evaluate(x, y) * 255;
				int bint = b.evaluate(x, y) * 255;
				Uint32 color = SDL_MapRGB(m_surface->format, rint, gint, bint);
/*				cout << "x:" << x << "  y:" << y << "  color:"<< color << endl;*/
				pixel->putpixel(m_surface, x, y, color);

			}
		}
	SDL_UpdateWindowSurface(m_window);
	cout << "完成渲染" << endl;
}

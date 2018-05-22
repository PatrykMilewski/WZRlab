#include <gl\gl.h>
#include <gl\glu.h>
#ifndef _WEKTOR__H
  #include "wektor.h"
#endif


enum GLDisplayListNames
{
	Wall1 = 1,
	Wall2 = 2,
	Floor = 3,
	Cube = 4,
	Cube_skel = 5,
	PowierzchniaTerenu = 6
};

// Ustwienia kamery w zale�no�ci od warto�ci pocz�tkowych (w interakcji), warto�ci ustawianych
// przez u�ytkowika oraz stanu obiektu (np. gdy tryb �ledzenia)
void UstawieniaKamery(Wektor3 *polozenie, Wektor3 *kierunek, Wektor3 *pion);

int InicjujGrafike(HDC g_context);
void RysujScene();
void ZmianaRozmiaruOkna(int cx,int cy);

Wektor3 WspolrzedneKursora3D(int x, int y); // wsp�rz�dne 3D punktu na obrazie 2D
Wektor3 WspolrzedneKursora3D(int x, int y, float wysokosc); // wsp�rz�dne 3D punktu na obrazie 2D
void WspolrzedneEkranu(float *xx, float *yy, float *zz, Wektor3 Punkt3D); // wsp�rz�dne punktu na ekranie na podstawie wsp 3D

void ZakonczenieGrafiki();

BOOL SetWindowPixelFormat(HDC hDC);
BOOL CreateViewGLContext(HDC hDC);
GLvoid BuildFont(HDC hDC);
GLvoid glPrint(const char *fmt, ...);
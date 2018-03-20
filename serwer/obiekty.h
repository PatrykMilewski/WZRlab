#include <stdio.h>
#include "kwaternion.h"

#define PI 3.1416

struct StanObiektu
{
  int iID;                  // identyfikator obiektu
	Wektor3 wPol;             // polozenie obiektu (œrodka geometrycznego obiektu) 
	kwaternion qOrient;       // orientacja (polozenie katowe)
	Wektor3 wV,wA;            // predkosc, przyspiesznie liniowe
  Wektor3 wV_kat, wA_kat;   // predkosc i przyspieszenie liniowe
  float masa;
};
// WZR_serwer.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <vector>
#include "siec.h"
#include "kwaternion.h"

struct StanObiektu
{
	int iID;                  // identyfikator obiektu
	Wektor3 wPol;             // polozenie obiektu (œrodka geometrycznego obiektu) 
	kwaternion qOrient;       // orientacja (polozenie katowe)
	Wektor3 wV, wA;            // predkosc, przyspiesznie liniowe
	Wektor3 wV_kat, wA_kat;   // predkosc i przyspieszenie liniowe
	float masa;
};

enum typy_ramek { STAN_OBIEKTU, INFO_O_ZAMKNIECIU, OFERTA };


struct Ramka                                    // g³ówna struktura s³u¿¹ca do przesy³ania informacji
{
	int typ;
	StanObiektu stan;
	float wartosc_oferty;
};

unicast_net *uni_receive;
unicast_net *uni_send;

std::vector<unsigned long> klienci;
Ramka ramka;
StanObiektu stan;

struct Klient {
	unsigned long add;
	int id;

	Klient(unsigned long x, int y) {
		add = x;
		id = y;
	}
};



int main()
{
	uni_receive = new unicast_net(10001);
	uni_send = new unicast_net(10002);

	while (1) {
		int rozmiar;
		unsigned long adres;
		rozmiar = uni_receive->reciv((char*)&ramka, &adres, sizeof(Ramka));
		bool found = false;
		switch (ramka.typ) {
		case typy_ramek::STAN_OBIEKTU:
			printf("stan");
			for (int i = 0; i < klienci.size(); i++) {
				if (klienci[i] == adres) {
					found = true;
					break;
				}
			}

			if (!found) {
				klienci.push_back(adres);
				printf("Connected %lu", adres);
			}

			for (int i = 0; i < klienci.size(); i++) {
				printf("To %lu (%d)\n", klienci[i], klienci.size());
				uni_send->send((char*)&ramka, klienci[i], sizeof(Ramka));
			}
			break;
		case typy_ramek::INFO_O_ZAMKNIECIU:
			stan = ramka.stan;

			for (int i = 0; i < klienci.size(); i++)
			{
				if (klienci[i] == adres) {
					printf("WYPIERDALAM TYPA Z SERWEERA !!!!!!!!! %d\n", klienci[i]);
					ramka.typ = INFO_O_ZAMKNIECIU;
					uni_send->send((char*)&ramka, klienci[i], sizeof(Ramka));
					for (int i = 0; i < klienci.size(); i++) {
						if (klienci[i] == adres) {
							klienci.erase(klienci.begin() + i);
						}
					}
				}

			}
			for (int i = 0; i < klienci.size(); i++)
			{
				if (klienci[i] == adres) {
					printf("Klient %ul SPIERDOLIL  \n", adres);
					klienci[i] = -1;
					//break;
				}
			}
			break;
		}

		
		if (klienci.size() == 0) {
			printf("BRAK TYCH SKURWYSYNOW!!!!!");
			break;
		}
	}

	return 0;
}



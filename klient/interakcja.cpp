/************************************************************
Interakcja:
Wysy³anie, odbiór komunikatów, interakcja z innymi
uczestnikami WZR, sterowanie wirtualnymi obiektami
*************************************************************/

#include <windows.h>
#include <time.h>
#include <stdio.h>   
#include "interakcja.h"
#include "obiekty.h"
#include "siec.h"
#include "grafika.h"

FILE *f = fopen("wzr_info.txt", "w");    // plik do zapisu informacji testowych


ObiektRuchomy *pMojObiekt;          // obiekt przypisany do tej aplikacji
Teren teren;
int iLiczbaCudzychOb = 0;
ObiektRuchomy *CudzeObiekty[4000];  // obiekty z innych aplikacji lub inne obiekty niz pMojObiekt
int IndeksyOb[4000];                // tablica indeksow innych obiektow ulatwiajaca wyszukiwanie

float fDt;                          // sredni czas pomiedzy dwoma kolejnymi cyklami symulacji i wyswietlania
long czas_cyklu_WS, licznik_sym;     // zmienne pomocnicze potrzebne do obliczania fDt
long czas_start = clock();          // czas uruchomienia aplikacji

unicast_net *multi_reciv;         // wsk do obiektu zajmujacego sie odbiorem komunikatow
unicast_net *multi_send;          //   -||-  wysylaniem komunikatow

unsigned long przychodzacy_adres_IP = 0;
char *adres_IP = "192.168.0.10";
unsigned long serwera_adres_IP = inet_addr(adres_IP);

HANDLE threadReciv;                 // uchwyt w¹tku odbioru komunikatów
extern HWND okno;
int SHIFTwcisniety = 0;
bool czy_rysowac_ID = 1;            // czy rysowac nr ID przy ka¿dym obiekcie
bool sterowanie_myszkowe = 0;       // sterowanie za pomoc¹ klawisza myszki
int kursor_x = 0, kursor_y = 0;     // po³o¿enie kursora myszy

// Parametry widoku:
Wektor3 kierunek_kamery_1 = Wektor3(11, -3, -14),   // kierunek patrzenia
pol_kamery_1 = Wektor3(-34, 6, 10),         // po³o¿enie kamery
pion_kamery_1 = Wektor3(0, 1, 0),           // kierunek pionu kamery        
kierunek_kamery_2 = Wektor3(0, -1, 0.02),   // to samo dla widoku z góry
pol_kamery_2 = Wektor3(0, 90, 0),
pion_kamery_2 = Wektor3(0, 0, -1),
kierunek_kamery = kierunek_kamery_1, pol_kamery = pol_kamery_1, pion_kamery = pion_kamery_1;
bool sledzenie = 1;                             // tryb œledzenia obiektu przez kamerê
bool widok_z_gory = 0;                          // tryb widoku z góry
float oddalenie = 36.0;                          // oddalenie widoku z kamery
float kat_kam_z = 0;                            // obrót kamery góra-dó³
float oddalenie_1 = oddalenie, kat_kam_z_1 = kat_kam_z, oddalenie_2 = oddalenie, kat_kam_z_2 = kat_kam_z,
oddalenie_3 = oddalenie, kat_kam_z_3 = kat_kam_z;


enum typy_ramek{ STAN_OBIEKTU, INFO_O_ZAMKNIECIU, OFERTA };

// rozbudowac, dodac info czy aplikacja zostala zamknieta przez uzytkownika
struct Ramka                                    // g³ówna struktura s³u¿¹ca do przesy³ania informacji
{
	int typ;
	StanObiektu stan;
	float wartosc_oferty;
};


//******************************************
// Funkcja obs³ugi w¹tku odbioru komunikatów 
DWORD WINAPI WatekOdbioru(void *ptr)
{
	unicast_net *pu_net = (unicast_net*)ptr;                // wskaŸnik do obiektu klasy unicast_net
	int rozmiar;                                               // liczba bajtów ramki otrzymanej z sieci
	Ramka ramka;
	StanObiektu stan;

	while (1)
	{
		rozmiar = pu_net->reciv((char*)&ramka, &przychodzacy_adres_IP, sizeof(Ramka));   // oczekiwanie na nadejœcie ramki - funkcja samoblokuj¹ca siê 
		switch (ramka.typ)
		{
		case STAN_OBIEKTU:
		{
			stan = ramka.stan;

			if (stan.iID != pMojObiekt->iID)                     // jeœli to nie mój obiekt
			{
				if (IndeksyOb[stan.iID] == -1)                     // nie ma jeszcze takiego obiektu w tablicy -> trzeba go stworzyæ
				{
					CudzeObiekty[iLiczbaCudzychOb] = new ObiektRuchomy(&teren);
					IndeksyOb[stan.iID] = iLiczbaCudzychOb;          // wpis do tablicy indeksowanej numerami ID
					// u³atwia wyszukiwanie, alternatyw¹ mo¿e byæ tabl. rozproszona                                                                                               
					iLiczbaCudzychOb++;
				}
				CudzeObiekty[IndeksyOb[stan.iID]]->ZmienStan(stan);   // zmiana stanu obiektu obcego 			
			}
			break;
		} // case STAN_OBIEKTU
		case OFERTA:
		{
			break;
		}
		case INFO_O_ZAMKNIECIU:          // zwyk³y komunikat tekstowy
		{
			stan = ramka.stan;
			CudzeObiekty[IndeksyOb[stan.iID]] = nullptr;
			IndeksyOb[stan.iID] -= 1;
			iLiczbaCudzychOb -= 1;
			break;
		}

		} // switch
	}  // while(1)
	return 1;
}

// *****************************************************************
// ****    Wszystko co trzeba zrobiæ podczas uruchamiania aplikacji
// ****    poza grafik¹   
void PoczatekInterakcji()
{
	DWORD dwThreadId;

	pMojObiekt = new ObiektRuchomy(&teren);    // tworzenie wlasnego obiektu

	for (long i = 0; i<4000; i++)            // inicjacja indeksow obcych obiektow
		IndeksyOb[i] = -1;

	czas_cyklu_WS = clock();             // pomiar aktualnego czasu

	// obiekty sieciowe typu unicast (z podaniem adresu WZR oraz numeru portu)
	multi_reciv = new unicast_net(10002);      // obiekt do odbioru ramek sieciowych
	multi_send = new unicast_net(10001);       // obiekt do wysy³ania ramek


	// uruchomienie watku obslugujacego odbior komunikatow
	threadReciv = CreateThread(
		NULL,                        // no security attributes
		0,                           // use default stack size
		WatekOdbioru,                // thread function
		(void *)multi_reciv,               // argument to thread function
		0,                           // use default creation flags
		&dwThreadId);                // returns the thread identifier

}


// *****************************************************************
// ****    Wszystko co trzeba zrobiæ w ka¿dym cyklu dzia³ania 
// ****    aplikacji poza grafik¹ 
void Cykl_WS()
{
	licznik_sym++;

	// obliczenie czasu fDt pomiedzy dwoma kolejnymi cyklami
	if (licznik_sym % 70 == 0)          // jeœli licznik cykli przekroczy³ pewn¹ wartoœæ, to
	{                                   // nale¿y na nowo obliczyæ œredni czas cyklu fDt
		char text[256];
		long czas_pop = czas_cyklu_WS;
		czas_cyklu_WS = clock();
		float fFps = (70 * CLOCKS_PER_SEC) / (float)(czas_cyklu_WS - czas_pop);
		if (fFps != 0) fDt = 1.0 / fFps; else fDt = 1;
		sprintf(text, "WZR-lab 2018 temat 1, wersja h, (%0.0f fps  %0.2fms)  ", fFps, 1000.0 / fFps);
		SetWindowText(okno, text); // wyœwietlenie aktualnej iloœci klatek/s w pasku okna			
	}


	pMojObiekt->Symulacja(fDt);                    // symulacja obiektu w³asnego 

	Ramka ramka;
	ramka.typ = STAN_OBIEKTU;
	ramka.stan = pMojObiekt->Stan();               // stan w³asnego obiektu 


	// wys³anie komunikatu o stanie obiektu przypisanego do aplikacji (pMojObiekt):    
	multi_send->send((char*)&ramka, serwera_adres_IP, sizeof(Ramka));
}

// *****************************************************************
// ****    Wszystko co trzeba zrobiæ podczas zamykania aplikacji
// ****    poza grafik¹ 
void ZakonczenieInterakcji()
{
	fprintf(f, "Interakcja zosta³a zakoñczona\n");
	Ramka ramka;
	ramka.typ = INFO_O_ZAMKNIECIU;
	ramka.stan = pMojObiekt->Stan();
	multi_send->send((char*)&ramka, adres_IP, sizeof(Ramka));
	fclose(f);
}


// ************************************************************************
// ****    Obs³uga klawiszy s³u¿¹cych do sterowania obiektami lub
// ****    widokami 
void KlawiszologiaSterowania(UINT kod_meldunku, WPARAM wParam, LPARAM lParam)
{

	switch (kod_meldunku)
	{

	case WM_LBUTTONDOWN: //reakcja na lewy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (sterowanie_myszkowe)
			pMojObiekt->F = 40.0;        // si³a pchaj¹ca do przodu
		break;
	}
	case WM_RBUTTONDOWN: //reakcja na prawy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (sterowanie_myszkowe)
			pMojObiekt->F = -8.0;        // si³a pchaj¹ca do tylu

		break;
	}
	case WM_MBUTTONDOWN: //reakcja na œrodkowy przycisk myszki : uaktywnienie/dezaktywacja sterwania myszkowego
	{
		sterowanie_myszkowe = 1 - sterowanie_myszkowe;
		kursor_x = LOWORD(lParam);
		kursor_y = HIWORD(lParam);
		break;
	}
	case WM_LBUTTONUP: //reakcja na puszczenie lewego przycisku myszki
	{
		if (sterowanie_myszkowe)
			pMojObiekt->F = 0.0;        // si³a pchaj¹ca do przodu
		break;
	}
	case WM_RBUTTONUP: //reakcja na puszczenie lewy przycisk myszki
	{
		if (sterowanie_myszkowe)
			pMojObiekt->F = 0.0;        // si³a pchaj¹ca do przodu
		break;
	}
	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (sterowanie_myszkowe)
		{
			float kat_skretu = (float)(kursor_x - x) / 20;
			if (kat_skretu > 45) kat_skretu = 45;
			if (kat_skretu < -45) kat_skretu = -45;
			pMojObiekt->alfa = PI*kat_skretu / 180;
		}
		break;
	}
	case WM_KEYDOWN:
	{

		switch (LOWORD(wParam))
		{
		case VK_SHIFT:
		{
			SHIFTwcisniety = 1;
			break;
		}
		case VK_SPACE:
		{
			pMojObiekt->ham = 10.0;       // stopieñ hamowania (reszta zale¿y od si³y docisku i wsp. tarcia)
			break;                       // 1.0 to maksymalny stopieñ (np. zablokowanie kó³)
		}
		case VK_UP:
		{

			pMojObiekt->F = 50.0;        // si³a pchaj¹ca do przodu
			break;
		}
		case VK_DOWN:
		{
			pMojObiekt->F = -15.0;
			break;
		}
		case VK_LEFT:
		{
			if (SHIFTwcisniety) pMojObiekt->alfa = PI * 25 / 180;
			else pMojObiekt->alfa = PI * 10 / 180;

			break;
		}
		case VK_RIGHT:
		{
			if (SHIFTwcisniety) pMojObiekt->alfa = -PI * 25 / 180;
			else pMojObiekt->alfa = -PI * 10 / 180;
			break;
		}
		case 'I':   // wypisywanie nr ID
		{
			czy_rysowac_ID = 1 - czy_rysowac_ID;
			break;
		}
		case 'W':   // oddalenie widoku
		{
			//pol_kamery = pol_kamery - kierunek_kamery*0.3;
			if (oddalenie > 0.5) oddalenie /= 1.2;
			else oddalenie = 0;
			break;
		}
		case 'S':   // przybli¿enie widoku
		{
			//pol_kamery = pol_kamery + kierunek_kamery*0.3; 
			if (oddalenie > 0) oddalenie *= 1.2;
			else oddalenie = 0.5;
			break;
		}
		case 'Q':   // widok z góry
		{
			//if (sledzenie) break;
			widok_z_gory = 1 - widok_z_gory;
			if (widok_z_gory)                // przechodzimy do widoku z góry
			{
				pol_kamery_1 = pol_kamery; kierunek_kamery_1 = kierunek_kamery; pion_kamery_1 = pion_kamery;
				oddalenie_1 = oddalenie; kat_kam_z_1 = kat_kam_z;

				//pol_kamery = pol_kamery_2; kierunek_kamery = kierunek_kamery_2; pion_kamery = pion_kamery_2;
				oddalenie = oddalenie_2; kat_kam_z = kat_kam_z_2;
				// Po³o¿enie kamery, kierunek oraz pion ustawiamy tak, by obiekt widziany by³ z góry i jecha³
				// pocz¹tkowo w górê ekranu:
				kierunek_kamery = (pMojObiekt->wPol - teren.srodek).znorm()*(-1);
				pol_kamery = pMojObiekt->wPol - kierunek_kamery*pMojObiekt->dlugosc * 10;
				pion_kamery = pMojObiekt->qOrient.obroc_wektor(Wektor3(1, 0, 0));
			}
			else
			{
				pol_kamery_2 = pol_kamery; kierunek_kamery_2 = kierunek_kamery; pion_kamery_2 = pion_kamery;
				oddalenie_2 = oddalenie; kat_kam_z_2 = kat_kam_z;

				// Po³o¿enie kamery, kierunek oraz pion ustawiamy tak, by obiekt widziany by³ z prawego boku i jecha³
				// pocz¹tkowo ze strony lewej na praw¹:
				kierunek_kamery = pMojObiekt->qOrient.obroc_wektor(Wektor3(0, 0, 1))*-1;
				pol_kamery = pMojObiekt->wPol - kierunek_kamery*pMojObiekt->dlugosc * 10;
				pion_kamery = (pMojObiekt->wPol - teren.srodek).znorm();

				//pol_kamery = pol_kamery_1; kierunek_kamery = kierunek_kamery_1; pion_kamery = pion_kamery_1;
				oddalenie = oddalenie_1; kat_kam_z = kat_kam_z_1;
			}
			break;
		}
		case 'E':   // obrót kamery ku górze (wzglêdem lokalnej osi z)
		{
			kat_kam_z += PI * 5 / 180;
			break;
		}
		case 'D':   // obrót kamery ku do³owi (wzglêdem lokalnej osi z)
		{
			kat_kam_z -= PI * 5 / 180;
			break;
		}
		case 'A':   // w³¹czanie, wy³¹czanie trybu œledzenia obiektu
		{
			sledzenie = 1 - sledzenie;
			if (sledzenie)
			{
				oddalenie = oddalenie_3; kat_kam_z = kat_kam_z_3;
			}
			else
			{
				oddalenie_3 = oddalenie; kat_kam_z_3 = kat_kam_z;
				widok_z_gory = 0;
				pol_kamery = pol_kamery_1; kierunek_kamery = kierunek_kamery_1; pion_kamery = pion_kamery_1;
				oddalenie = oddalenie_1; kat_kam_z = kat_kam_z_1;
			}
			break;
		}
		case VK_ESCAPE:
		{
			SendMessage(okno, WM_DESTROY, 0, 0);
			break;
		}
		} // switch po klawiszach

		break;
	}
	case WM_KEYUP:
	{
		switch (LOWORD(wParam))
		{
		case VK_SHIFT:
		{
			SHIFTwcisniety = 0;
			break;
		}
		case VK_SPACE:
		{
			pMojObiekt->ham = 0.0;
			break;
		}
		case VK_UP:
		{
			pMojObiekt->F = 0.0;

			break;
		}
		case VK_DOWN:
		{
			pMojObiekt->F = 0.0;
			break;
		}
		case VK_LEFT:
		{
			pMojObiekt->Fb = 0.00;
			pMojObiekt->alfa = 0;
			break;
		}
		case VK_RIGHT:
		{
			pMojObiekt->Fb = 0.00;
			pMojObiekt->alfa = 0;
			break;
		}

		}

		break;
	}

	} // switch po komunikatach
}

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
#include "grafika.h"
#include "siec.h"


void wyslijPropozycjeZamiany(int ostatniaCyfraID);

bool czy_umiejetnosci = true;          // czy zró¿nicowanie umiejêtnoœci (dla ka¿dego pojazdu losowane s¹ umiejêtnoœci
// zbierania gotówki i paliwa)


FILE *f = fopen("wzr_log.txt", "w");     // plik do zapisu informacji testowych

ObiektRuchomy *pMojObiekt;             // obiekt przypisany do tej aplikacji

Teren teren;
int iLiczbaCudzychOb = 0;
ObiektRuchomy *CudzeObiekty[1000];  // obiekty z innych aplikacji lub inne obiekty niz w³asny obiekt
int IndeksyOb[1000];                // tablica indeksow innych obiektow ulatwiajaca wyszukiwanie


float fDt;                          // sredni czas pomiedzy dwoma kolejnymi cyklami symulacji i wyswietlania
long czas_cyklu_WS, licznik_sym;     // zmienne pomocnicze potrzebne do obliczania fDt
float sr_czestosc;                  // srednia czestosc wysylania ramek w [ramkach/s] 
long czas_start = clock();          // czas od poczatku dzialania aplikacji  
long czas_istnienia_grupy = clock();    // czas od pocz¹tku istnienia grupy roboczej (czas od uruchom. pierwszej aplikacji)      

multicast_net *multi_reciv;         // wsk do obiektu zajmujacego sie odbiorem komunikatow
multicast_net *multi_send;          //   -||-  wysylaniem komunikatow

HANDLE threadReciv;                 // uchwyt w¹tku odbioru komunikatów
extern HWND okno;
bool SHIFTwcisniety = 0;
bool CTRLwcisniety = 0;
bool ALTwcisniety = 0;
bool Lwcisniety = 0;
bool rejestracja_uczestnikow = true;   // rejestracja trwa do momentu wziêcia przedmiotu przez któregokolwiek uczestnika,
// w przeciwnym razie trzeba by przesy³aæ ca³y stan œrodowiska nowicjuszowi

// Parametry widoku:
// Stale (wartości początkowe)
Wektor3 pocz_kierunek_kamery = Wektor3(0, -3, -11);   // kierunek patrzenia
Wektor3 pocz_pol_kamery = Wektor3(30, 3, 0);          // po³o¿enie kamery
Wektor3 pocz_pion_kamery = Wektor3(0, 1, 0);           // kierunek pionu kamery             

// Zmienne - ustawiane przez użytkownika
bool sledzenie = 1;                             // tryb œledzenia obiektu przez kamerê
bool widok_z_gory = 0;                          // tryb widoku z gory
float oddalenie = 10.0;                          // oddalenie lub przybli¿enie kamery
float zoom = 1.0;                               // zmiana kąta widzenia
float kat_kam_z = 0;                            // obrót kamery góra-dół
bool sterowanie_myszkowe = 0;                   // sterowanie pojazdem za pomoc¹ myszki
float przes_w_prawo = 0;                        // przesunięcie kamery w prawo (w lewo o wart. ujemnej) - chodzi głównie o tryb edycji
float przes_w_dol = 0;                          // przesunięcie do dołu (w górę o wart. ujemnej)          i widok z góry (klawisz Q)  

int kursor_x, kursor_y;                         // polo¿enie kursora myszki w chwili w³¹czenia sterowania
char napis1[300], napis2[300];                  // napisy wyœwietlane w trybie graficznym 
long nr_miejsca_przedm = teren.liczba_przedmiotow;  // numer miejsca, w którym mo¿na umieœciæ przedmiot

int opoznienia = 0;

bool tryb_pokazu_autopilota = 0;                // czy pokazywać test autopilota
float biezacy_czas_pokazu_autopilota = 0;       // czas jaki upłynął od początku testu autopilota
float krok_czasowy_autopilota = 0.01;           // stały krok czasowy wolny od możliwości sprzętowych (zamiast fDt)
float czas_testu_autopilota = 600;              // całkowity czas testu

extern float WyslaniePrzekazu(int ID_adresata, int typ_przekazu, float wartosc_przekazu);

enum typy_ramek {
	STAN_OBIEKTU, WZIECIE_PRZEDMIOTU, ODNOWIENIE_SIE_PRZEDMIOTU, KOLIZJA, PRZEKAZ,
	PROSBA_O_ZAMKNIECIE, NEGOCJACJE_HANDLOWE, OFERTA_TEAM, OFERTA_TEAM_AKCEPTACJA, OFERTA_TEAM_ODRZUCENIE
};

enum typy_przekazu { GOTOWKA, PALIWO };

struct Ramka
{
	int iID;
	int typ_ramki;
	long czas_wyslania;
	int iID_adresata;      // nr ID adresata wiadomoœci (pozostali uczestnicy powinni wiadomoœæ zignorowaæ

	int nr_przedmiotu;     // nr przedmiotu, który zosta³ wziêty lub odzyskany
	Wektor3 wdV_kolid;     // wektor prêdkoœci wyjœciowej po kolizji (uczestnik o wskazanym adresie powinien 
	// przyj¹æ t¹ prêdkoœæ)  

	int typ_przekazu;        // gotówka, paliwo
	float wartosc_przekazu;  // iloœæ gotówki lub paliwa 
	int nr_druzyny;

	StanObiektu stan;

	long czas_istnienia;        // czas jaki uplyn¹³ od uruchomienia programu
};


//******************************************
// Funkcja obs³ugi w¹tku odbioru komunikatów 
DWORD WINAPI WatekOdbioru(void *ptr)
{
	multicast_net *pmt_net = (multicast_net*)ptr;  // wskaŸnik do obiektu klasy multicast_net
	int rozmiar;                                 // liczba bajtów ramki otrzymanej z sieci
	Ramka ramka;
	StanObiektu stan;

	while (1)
	{
		rozmiar = pmt_net->reciv((char*)&ramka, sizeof(Ramka));   // oczekiwanie na nadejœcie ramki 
		switch (ramka.typ_ramki)
		{
		case STAN_OBIEKTU:           // podstawowy typ ramki informuj¹cej o stanie obiektu              
		{
			stan = ramka.stan;
			//fprintf(f,"odebrano stan iID = %d, ID dla mojego obiektu = %d\n",stan.iID,pMojObiekt->iID);
			int niewlasny = 1;
			if ((ramka.iID != pMojObiekt->iID))          // jeœli to nie mój w³asny obiekt
			{

				if ((rejestracja_uczestnikow) && (IndeksyOb[ramka.iID] == -1))        // nie ma jeszcze takiego obiektu w tablicy -> trzeba go stworzyæ
				{
					CudzeObiekty[iLiczbaCudzychOb] = new ObiektRuchomy(&teren);
					IndeksyOb[ramka.iID] = iLiczbaCudzychOb;     // wpis do tablicy indeksowanej numerami ID
					// u³atwia wyszukiwanie, alternatyw¹ mo¿e byæ tabl. rozproszona                                                                                                           
					iLiczbaCudzychOb++;
					if (ramka.czas_istnienia > czas_istnienia_grupy) czas_istnienia_grupy = ramka.czas_istnienia;

					CudzeObiekty[IndeksyOb[ramka.iID]]->ZmienStan(stan);   // aktualizacja stanu obiektu obcego 
					teren.WstawObiektWsektory(CudzeObiekty[IndeksyOb[ramka.iID]]);
					// wys³anie nowemu uczestnikowi informacji o wszystkich wziêtych przedmiotach:
					for (long i = 0; i < teren.liczba_przedmiotow; i++)
						if ((teren.p[i].do_wziecia == 0) && (teren.p[i].czy_ja_wzialem))
						{
							Ramka ramka;
							ramka.typ_ramki = WZIECIE_PRZEDMIOTU;
							ramka.nr_przedmiotu = i;
							ramka.stan = pMojObiekt->Stan();
							ramka.iID = pMojObiekt->iID;
							int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));
						}

				}
				else if (IndeksyOb[ramka.iID] != -1)
				{
					teren.UsunObiektZsektorow(CudzeObiekty[IndeksyOb[ramka.iID]]);
					CudzeObiekty[IndeksyOb[ramka.iID]]->ZmienStan(stan);   // aktualizacja stanu obiektu obcego 	
					teren.WstawObiektWsektory(CudzeObiekty[IndeksyOb[ramka.iID]]);

				}
				else if (rejestracja_uczestnikow == false)              // koniec rejestracji                                        
				{
					Ramka ramka;
					ramka.typ_ramki = PROSBA_O_ZAMKNIECIE;                // ¿¹danie zamkniêcia aplikacji
					ramka.iID_adresata = ramka.iID;
					int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));
				}
			}
			break;
		}
		case WZIECIE_PRZEDMIOTU:            // ramka informuj¹ca, ¿e ktoœ wzi¹³ przedmiot o podanym numerze
		{
			stan = ramka.stan;
			if ((ramka.nr_przedmiotu < teren.liczba_przedmiotow) && (ramka.iID != pMojObiekt->iID))
			{
				teren.p[ramka.nr_przedmiotu].do_wziecia = 0;
				teren.p[ramka.nr_przedmiotu].czy_ja_wzialem = 0;
				//rejestracja_uczestnikow = 0;
			}
			break;
		}
		case ODNOWIENIE_SIE_PRZEDMIOTU:       // ramka informujaca, ¿e przedmiot wczeœniej wziêty pojawi³ siê znowu w tym samym miejscu
		{
			if (ramka.nr_przedmiotu < teren.liczba_przedmiotow)
				teren.p[ramka.nr_przedmiotu].do_wziecia = 1;
			break;
		}
		case KOLIZJA:                       // ramka informuj¹ca o tym, ¿e obiekt uleg³ kolizji
		{
			if (ramka.iID_adresata == pMojObiekt->iID)  // ID pojazdu, który uczestniczy³ w kolizji zgadza siê z moim ID 
			{
				pMojObiekt->wdV_kolid = ramka.wdV_kolid; // przepisuje poprawkê w³asnej prêdkoœci
				pMojObiekt->iID_kolid = pMojObiekt->iID; // ustawiam nr. kolidujacego jako w³asny na znak, ¿e powinienem poprawiæ prêdkoœæ
			}
			break;
		}
		case PRZEKAZ:                       // ramka informuj¹ca o przelewie pieniê¿nym lub przekazaniu towaru    
		{
			if (ramka.iID_adresata == pMojObiekt->iID)  // ID pojazdu, ktory otrzymal przelew zgadza siê z moim ID 
			{
				if (ramka.typ_przekazu == GOTOWKA)
					pMojObiekt->pieniadze += ramka.wartosc_przekazu;
				else if (ramka.typ_przekazu == PALIWO)
					pMojObiekt->ilosc_paliwa += ramka.wartosc_przekazu;

				// nale¿a³oby jeszcze przelew potwierdziæ (w UDP ramki mog¹ byæ gubione!)
			}
			break;
		}
		case PROSBA_O_ZAMKNIECIE:           // ramka informuj¹ca, ¿e powinieneœ siê zamkn¹æ
		{
			if (ramka.iID_adresata == pMojObiekt->iID)
			{
				SendMessage(okno, WM_DESTROY, 0, 100);
			}
			break;
		}
		case NEGOCJACJE_HANDLOWE:
		{
			// ------------------------------------------------------------------------
			// --------------- MIEJSCE #1 NA NEGOCJACJE HANDLOWE  ---------------------
			// (szczegó³y na stronie w instrukcji do zadania)




			// ------------------------------------------------------------------------

			break;
		}

		case OFERTA_TEAM:
		{
			if (ramka.iID_adresata == pMojObiekt->iID) {
				int msgboxID = MessageBox(
					NULL,
					"Zamiana",
					"Czy chcesz dolaczyc do druzyny?",
					MB_OKCANCEL);
				if (msgboxID == 1) {
					ramka.typ_ramki = OFERTA_TEAM_AKCEPTACJA;
					multi_send->send((char*)&ramka, sizeof(Ramka));

					pMojObiekt->IDPartnera = ramka.stan.iID;


				}
			}
			break;
		}

		case OFERTA_TEAM_AKCEPTACJA:
		{

			if (ramka.stan.iID == pMojObiekt->iID) {
				
				pMojObiekt->IDPartnera = ramka.iID_adresata;

			}
			else {
				
				for (int i = 0; i < iLiczbaCudzychOb; i++) {
					if (CudzeObiekty[i]->iID == ramka.stan.iID) {
						CudzeObiekty[i]->IDPartnera = ramka.iID_adresata;
					}
					else if (CudzeObiekty[i]->iID == ramka.iID_adresata)
						CudzeObiekty[i]->IDPartnera = ramka.stan.iID;

				}

			}

			break;
		}

		case OFERTA_TEAM_ODRZUCENIE:
		{
			printf("Odrzucono oferte teamu.");
			break;
		}

		} // switch po typach ramek
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
	if (czy_umiejetnosci == false)
		pMojObiekt->umiejetn_sadzenia = pMojObiekt->umiejetn_zb_monet = pMojObiekt->umiejetn_zb_paliwa = 1.0;


	for (long i = 0; i < 1000; i++)            // inicjacja indeksow obcych obiektow
		IndeksyOb[i] = -1;

	czas_cyklu_WS = clock();             // pomiar aktualnego czasu

	// obiekty sieciowe typu multicast (z podaniem adresu WZR oraz numeru portu)
	multi_reciv = new multicast_net("224.10.12.65", 10001);      // obiekt do odbioru ramek sieciowych
	multi_send = new multicast_net("224.10.12.65", 10001);       // obiekt do wysy³ania ramek

	if (opoznienia)
	{
		float srednie_opoznienie = 3 * (float)rand() / RAND_MAX, wariancja_opoznienia = 2;
		multi_send->PrepareDelay(srednie_opoznienie, wariancja_opoznienia);
	}

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

	// obliczenie œredniego czasu pomiêdzy dwoma kolejnnymi symulacjami po to, by zachowaæ  fizycznych 
	if (licznik_sym % 50 == 0)          // jeœli licznik cykli przekroczy³ pewn¹ wartoœæ, to
	{                                   // nale¿y na nowo obliczyæ œredni czas cyklu fDt
		char text[200];
		long czas_pop = czas_cyklu_WS;
		czas_cyklu_WS = clock();
		float fFps = (50 * CLOCKS_PER_SEC) / (float)(czas_cyklu_WS - czas_pop);
		if (fFps != 0) fDt = 1.0 / fFps; else fDt = 1;

		sprintf(napis1, " %0.0f_fps, paliwo = %0.2f, gotowka = %d,", fFps, pMojObiekt->ilosc_paliwa, pMojObiekt->pieniadze);
		if (licznik_sym % 500 == 0) sprintf(napis2, "");
	}

	teren.UsunObiektZsektorow(pMojObiekt);
	pMojObiekt->Symulacja(fDt, CudzeObiekty, iLiczbaCudzychOb);                    // symulacja w³asnego obiektu
	teren.WstawObiektWsektory(pMojObiekt);



	if ((pMojObiekt->iID_kolid > -1) &&             // wykryto kolizjê - wysy³am specjaln¹ ramkê, by poinformowaæ o tym drugiego uczestnika
		(pMojObiekt->iID_kolid != pMojObiekt->iID)) // oczywiœcie wtedy, gdy nie chodzi o mój pojazd
	{
		Ramka ramka;
		ramka.typ_ramki = KOLIZJA;
		ramka.iID_adresata = pMojObiekt->iID_kolid;
		ramka.wdV_kolid = pMojObiekt->wdV_kolid;
		ramka.iID = pMojObiekt->iID;
		int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));

		char text[128];
		sprintf(napis2, "Kolizja_z_obiektem_o_ID = %d", pMojObiekt->iID_kolid);
		//SetWindowText(okno,text);

		pMojObiekt->iID_kolid = -1;
	}

	// wyslanie komunikatu o stanie obiektu przypisanego do aplikacji (pMojObiekt):    
	if (licznik_sym % 1 == 0)
	{
		Ramka ramka;
		ramka.typ_ramki = STAN_OBIEKTU;
		ramka.stan = pMojObiekt->Stan();         // stan w³asnego obiektu 
		ramka.iID = pMojObiekt->iID;
		ramka.czas_istnienia = clock() - czas_start;
		int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));
	}


	// wziêcie przedmiotu -> wysy³anie ramki 
	if (pMojObiekt->nr_wzietego_przedm > -1)
	{
		Ramka ramka;
		ramka.typ_ramki = WZIECIE_PRZEDMIOTU;
		ramka.nr_przedmiotu = pMojObiekt->nr_wzietego_przedm;
		ramka.stan = pMojObiekt->Stan();
		ramka.iID = pMojObiekt->iID;
		int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));

		sprintf(napis2, "Wziecie_przedmiotu_o_wartosci_ %f", pMojObiekt->wartosc_wzieta);

		pMojObiekt->nr_wzietego_przedm = -1;
		pMojObiekt->wartosc_wzieta = 0;
		//rejestracja_uczestnikow = 0;     // koniec rejestracji nowych uczestników
	}

	// odnawianie siê przedmiotu -> wysy³anie ramki
	if (pMojObiekt->nr_odnowionego_przedm > -1)
	{                             // jeœli min¹³ pewnien okres czasu przedmiot mo¿e zostaæ przywrócony
		Ramka ramka;
		ramka.typ_ramki = ODNOWIENIE_SIE_PRZEDMIOTU;
		ramka.nr_przedmiotu = pMojObiekt->nr_odnowionego_przedm;
		ramka.iID = pMojObiekt->iID;
		int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));


		pMojObiekt->nr_odnowionego_przedm = -1;
	}




	// --------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// --------------- MIEJSCE #2 NA NEGOCJACJE HANDLOWE  ---------------------
	// (szczegó³y na stronie w instrukcji do zadania)




	// ------------------------------------------------------------------------
}

// *****************************************************************
// ****    Wszystko co trzeba zrobiæ podczas zamykania aplikacji
// ****    poza grafik¹ 
void ZakonczenieInterakcji()
{
	fprintf(f, "Koniec interakcji\n");
	fclose(f);
}

// Funkcja wysylajaca ramke z przekazem, zwraca zrealizowan¹ wartoœæ przekazu
float WyslaniePrzekazu(int ID_adresata, int typ_przekazu, float wartosc_przekazu)
{
	Ramka ramka;
	ramka.typ_ramki = PRZEKAZ;
	ramka.iID_adresata = ID_adresata;
	ramka.typ_przekazu = typ_przekazu;
	ramka.wartosc_przekazu = wartosc_przekazu;
	ramka.iID = pMojObiekt->iID;

	// tutaj nale¿a³oby uzyskaæ potwierdzenie przekazu zanim sumy zostan¹ odjête
	if (typ_przekazu == GOTOWKA)
	{
		if (pMojObiekt->pieniadze < wartosc_przekazu)
			ramka.wartosc_przekazu = pMojObiekt->pieniadze;
		pMojObiekt->pieniadze -= ramka.wartosc_przekazu;
		sprintf(napis2, "Przelew_sumy_ %f _na_rzecz_ID_ %d", wartosc_przekazu, ID_adresata);
	}
	else if (typ_przekazu == PALIWO)
	{
		// odszukanie adresata, sprawdzenie czy jest odpowiednio blisko:
		int indeks_adresata = -1;
		for (int i = 0; i<iLiczbaCudzychOb; i++)
			if (CudzeObiekty[i]->iID == ID_adresata) { indeks_adresata = i; break; }
		if ((CudzeObiekty[indeks_adresata]->wPol - pMojObiekt->wPol).dlugosc() >
			CudzeObiekty[indeks_adresata]->dlugosc + pMojObiekt->dlugosc)
			ramka.wartosc_przekazu = 0;
		else
		{
			if (pMojObiekt->ilosc_paliwa < wartosc_przekazu)
				ramka.wartosc_przekazu = pMojObiekt->ilosc_paliwa;
			pMojObiekt->ilosc_paliwa -= ramka.wartosc_przekazu;
			sprintf(napis2, "Przekazanie_paliwa_w_iloœci_ %f _na_rzecz_ID_ %d", wartosc_przekazu, ID_adresata);
		}
	}

	if (ramka.wartosc_przekazu > 0)
		int iRozmiar = multi_send->send_delayed((char*)&ramka, sizeof(Ramka));

	return ramka.wartosc_przekazu;
}


// ************************************************************************
// ****    Obs³uga klawiszy s³u¿¹cych do sterowania obiektami lub
// ****    widokami 
void KlawiszologiaSterowania(UINT kod_meldunku, WPARAM wParam, LPARAM lParam)
{

	int LCONTROL = GetKeyState(VK_LCONTROL);
	int RCONTROL = GetKeyState(VK_RCONTROL);
	int LALT = GetKeyState(VK_LMENU);
	int RALT = GetKeyState(VK_RMENU);


	switch (kod_meldunku)
	{

	case WM_LBUTTONDOWN: //reakcja na lewy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (sterowanie_myszkowe)
			pMojObiekt->F = pMojObiekt->F_max;        // si³a pchaj¹ca do przodu

		break;
	}
	case WM_RBUTTONDOWN: //reakcja na prawy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		int LSHIFT = GetKeyState(VK_LSHIFT);   // sprawdzenie czy lewy Shift wciśnięty, jeśli tak, to LSHIFT == 1
		int RSHIFT = GetKeyState(VK_RSHIFT);

		if (sterowanie_myszkowe)
			pMojObiekt->F = -pMojObiekt->F_max / 2;        // si³a pchaj¹ca do tylu
		else if (wParam & MK_SHIFT)                    // odznaczanie wszystkich obiektów   
		{
			for (long i = 0; i < teren.liczba_zazn_przedm; i++)
				teren.p[teren.zazn_przedm[i]].czy_zazn = 0;
			teren.liczba_zazn_przedm = 0;
		}
		else                                          // zaznaczenie obiektów
		{
			RECT r;
			//GetWindowRect(okno,&r);
			GetClientRect(okno, &r);
			//Wektor3 w = WspolrzedneKursora3D(x, r.bottom - r.top - y);
			Wektor3 w = teren.wspolrzedne_kursora3D_bez_paralaksy(x, r.bottom - r.top - y);

			
			//float promien = (w - punkt_klik).dlugosc();
			float odl_min = 1e10;
			long ind_min = -1;
			bool czy_ob_ruch;
			for (long i = 0; i < iLiczbaCudzychOb; i++)
			{
				float xx, yy, zz;
				WspolrzedneEkranu(&xx, &yy, &zz, CudzeObiekty[i]->wPol);
				yy = r.bottom - r.top - yy;
				float odl_kw = (xx - x)*(xx - x) + (yy - y)*(yy - y);
				if (odl_min > odl_kw)
				{
					odl_min = odl_kw;
					ind_min = i;
					czy_ob_ruch = 1;
				}
			}


			/*ObiektRuchomy **wsk_ob = NULL;
			long liczba_ob_w_promieniu = teren.Obiekty_w_promieniu(&wsk_ob, w, 100);

			for (long i = 0; i < liczba_ob_w_promieniu; i++)
			{
				float xx, yy, zz;
				WspolrzedneEkranu(&xx, &yy, &zz, wsk_ob[i]->wPol);
				yy = r.bottom - r.top - yy;
				float odl_kw = (xx - x)*(xx - x) + (yy - y)*(yy - y);
				if (odl_min > odl_kw)
				{
					odl_min = odl_kw;
					ind_min = IndeksyOb[wsk_ob[i]->iID];
					czy_ob_ruch = 1;
				}
			}
			
			delete wsk_ob;
			*/

			// trzeba to przerobić na wersję sektorową, gdyż przedmiotów może być dużo!
			// niestety nie jest to proste. 

			//Przedmiot **wsk_prz = NULL;
			//long liczba_prz_w_prom = teren.Przedmioty_w_promieniu(&wsk_prz, w,100);

			for (long i = 0; i < teren.liczba_przedmiotow; i++)
			{
				float xx, yy, zz;
				Wektor3 polozenie;
				if ((teren.p[i].typ == PRZ_KRAWEDZ) || (teren.p[i].typ == PRZ_MUR))
				{
					polozenie = (teren.p[teren.p[i].param_i[0]].wPol + teren.p[teren.p[i].param_i[1]].wPol) / 2;
				}
				else
					polozenie = teren.p[i].wPol;
				WspolrzedneEkranu(&xx, &yy, &zz, polozenie);
				yy = r.bottom - r.top - yy;
				float odl_kw = (xx - x)*(xx - x) + (yy - y)*(yy - y);
				if (odl_min > odl_kw)
				{
					odl_min = odl_kw;
					ind_min = i;
					czy_ob_ruch = 0;
				}
			}

			if (ind_min > -1)
			{
				//fprintf(f,"zaznaczono przedmiot %d pol = (%f, %f, %f)\n",ind_min,teren.p[ind_min].wPol.x,teren.p[ind_min].wPol.y,teren.p[ind_min].wPol.z);
				//teren.p[ind_min].czy_zazn = 1 - teren.p[ind_min].czy_zazn;
				if (czy_ob_ruch)
				{
					CudzeObiekty[ind_min]->czy_zazn = 1 - CudzeObiekty[ind_min]->czy_zazn;
					if (CudzeObiekty[ind_min]->czy_zazn)
						sprintf(napis2, "zaznaczono_ obiekt_ID_%d", CudzeObiekty[ind_min]->iID);
				}
				else
				{
					teren.ZaznaczOdznaczPrzedmiotLubGrupe(ind_min);
				}
				//char lan[256];
				//sprintf(lan, "klikniêto w przedmiot %d pol = (%f, %f, %f)\n",ind_min,teren.p[ind_min].wPol.x,teren.p[ind_min].wPol.y,teren.p[ind_min].wPol.z);
				//SetWindowText(okno,lan);
			}
			Wektor3 punkt_klik = WspolrzedneKursora3D(x, r.bottom - r.top - y);

		}

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
	case WM_MOUSEWHEEL:     // ruch kó³kiem myszy -> przybli¿anie, oddalanie widoku
	{
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);  // dodatni do przodu, ujemny do ty³u
		//fprintf(f,"zDelta = %d\n",zDelta);          // zwykle +-120, jak siê bardzo szybko zakrêci to czasmi wyjdzie +-240
		if (zDelta > 0){
			if (oddalenie > 0.5) oddalenie /= 1.2;
			else oddalenie = 0;
		}
		else {
			if (oddalenie > 0) oddalenie *= 1.2;
			else oddalenie = 0.5;
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
		case VK_CONTROL:
		{
			CTRLwcisniety = 1;
			break;
		}
		case VK_MENU:
		{
			ALTwcisniety = 1;
			break;
		}

		case VK_SPACE:
		{
			pMojObiekt->ham = 1.0;       // stopieñ hamowania (reszta zale¿y od si³y docisku i wsp. tarcia)
			break;                       // 1.0 to maksymalny stopieñ (np. zablokowanie kó³)
		}
		case VK_UP:
		{
			if (CTRLwcisniety && widok_z_gory)
				przes_w_dol += oddalenie/2;       // przesunięcie widoku z kamery w górę
			else
				pMojObiekt->F = pMojObiekt->F_max;        // si³a pchaj¹ca do przodu
			break;
		}
		case VK_DOWN:
		{
			if (CTRLwcisniety && widok_z_gory)
				przes_w_dol -= oddalenie/2;       // przesunięcie widoku z kamery w dół 
			else
				pMojObiekt->F = -pMojObiekt->F_max / 2;        // sila pchajaca do tylu
			break;
		}
		case VK_LEFT:
		{
			if (CTRLwcisniety && widok_z_gory)
				przes_w_prawo += oddalenie/2;       // przesunięcie widoku z kamery w lewo
			else                       
			{                           // skręt pojazdu w lewo
				if (wParam & SHIFTwcisniety) pMojObiekt->alfa = PI * 35 / 180;
				else
					pMojObiekt->alfa = PI * 15 / 180;
			}
			break;
		}
		case VK_RIGHT:
		{
			if (CTRLwcisniety && widok_z_gory)
			{
				przes_w_prawo -= oddalenie/2;       // przesunięcie widoku z kamery w prawo
			}
			else
			{
				if (wParam & SHIFTwcisniety) pMojObiekt->alfa = -PI * 35 / 180;
				else
					pMojObiekt->alfa = -PI * 15 / 180;
			}
			break;
		}
		case VK_HOME:
		{
			if (CTRLwcisniety && widok_z_gory)
				przes_w_prawo = przes_w_dol = 0;
		
			break;
		}
		case 'W':   // przybli¿enie widoku
		{
			//pocz_pol_kamery = pocz_pol_kamery - pocz_kierunek_kamery*0.3;
			if (oddalenie > 0.5) oddalenie /= 1.2;
			else oddalenie = 0;
			break;
		}
		case 'S':   // oddalenie widoku
		{
			//pocz_pol_kamery = pocz_pol_kamery + pocz_kierunek_kamery*0.3; 
			if (oddalenie > 0) oddalenie *= 1.2;
			else oddalenie = 0.5;
			break;
		}
		case 'Q':   // widok z góry
		{
			widok_z_gory = 1 - widok_z_gory;
			if (widok_z_gory)
				SetWindowText(okno, "Włączono widok z góry!");
			else
				SetWindowText(okno, "Wyłączono widok z góry.");
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
			break;
		}
		case 'Z':   // zoom - zmniejszenie k¹ta widzenia
		{
			zoom /= 1.1;
			RECT rc;
			GetClientRect(okno, &rc);
			ZmianaRozmiaruOkna(rc.right - rc.left, rc.bottom - rc.top);
			break;
		}
		case 'X':   // zoom - zwiêkszenie k¹ta widzenia
		{
			zoom *= 1.1;
			RECT rc;
			GetClientRect(okno, &rc);
			ZmianaRozmiaruOkna(rc.right - rc.left, rc.bottom - rc.top);
			break;
		}
		case 'O':   // zanotowanie w pliku po³o¿enia œrodka pojazdu jako miejsca posadzenia drzewa
		{
			//fprintf(f,"p[%d].typ = PRZ_DRZEWO; p[%d].podtyp = DRZ_SWIERK; p[%d].wPol = Wektor3(%f,%f,%f); p[%d].do_wziecia = 0; p[%d].wartosc = 8;\n",
			//  nr_miejsca_przedm,nr_miejsca_przedm,nr_miejsca_przedm,pMojObiekt->wPol.x,pMojObiekt->wPol.y,pMojObiekt->wPol.z,
			//  nr_miejsca_przedm,nr_miejsca_przedm);
			fprintf(f, "p[%d].typ = PRZ_MONETA; p[%d].wPol = Wektor3(%f,%f,%f); p[%d].do_wziecia = 1; p[%d].wartosc = 200;\n",
				nr_miejsca_przedm, nr_miejsca_przedm, pMojObiekt->wPol.x, pMojObiekt->wPol.y, pMojObiekt->wPol.z,
				nr_miejsca_przedm, nr_miejsca_przedm);
			//fprintf(f,"p[%d].typ = PRZ_BECZKA; p[%d].wPol = Wektor3(%f,%f,%f); p[%d].do_wziecia = 1; p[%d].wartosc = 10;\n",
			//  nr_miejsca_przedm,nr_miejsca_przedm,pMojObiekt->wPol.x,pMojObiekt->wPol.y,pMojObiekt->wPol.z,
			//  nr_miejsca_przedm,nr_miejsca_przedm);
			nr_miejsca_przedm++;
			break;
		}

		case 'F':  // przekazanie 10 kg paliwa pojazdowi, który znajduje siê najbli¿ej
		{
			float min_odleglosc = 1e10;
			int indeks_min = -1;
			for (int i = 0; i<iLiczbaCudzychOb; i++)
			{
				if (min_odleglosc >(CudzeObiekty[i]->wPol - pMojObiekt->wPol).dlugosc())
				{
					min_odleglosc = (CudzeObiekty[i]->wPol - pMojObiekt->wPol).dlugosc();
					indeks_min = i;
				}
			}

			float ilosc_p = 0;
			if (indeks_min > -1)
				ilosc_p = WyslaniePrzekazu(CudzeObiekty[indeks_min]->iID, PALIWO, 10);

			if (ilosc_p == 0)
				MessageBox(okno, "Paliwa nie da³o siê przekazaæ, bo byæ mo¿e najbli¿szy obiekt ruchomy znajduje siê zbyt daleko.",
				"Nie da³o siê przekazaæ paliwa!", MB_OK);
			break;
		}
		case 'G':  // przekazanie 100 jednostek gotowki pojazdowi, który znajduje siê najbli¿ej
		{
			float min_odleglosc = 1e10;
			int indeks_min = -1;
			for (int i = 0; i<iLiczbaCudzychOb; i++)
			{
				if (min_odleglosc >(CudzeObiekty[i]->wPol - pMojObiekt->wPol).dlugosc())
				{
					min_odleglosc = (CudzeObiekty[i]->wPol - pMojObiekt->wPol).dlugosc();
					indeks_min = i;
				}
			}

			float ilosc_p = 0;
			if (indeks_min > -1)
				ilosc_p = WyslaniePrzekazu(CudzeObiekty[indeks_min]->iID, GOTOWKA, 100);

			if (ilosc_p == 0)
				MessageBox(okno, "Gotówki nie da³o siê przekazaæ, bo byæ mo¿e najbli¿szy obiekt ruchomy znajduje siê zbyt daleko.",
				"Nie da³o siê przekazaæ gotówki!", MB_OK);
			break;
		}
		case '1':
		{
			wyslijPropozycjeZamiany(1);
			break;
		}
		case '2':
		{
			wyslijPropozycjeZamiany(2);
			break;
		}
		case '3':
		{
			wyslijPropozycjeZamiany(3);
			break;
		}
		case '4':
		{
			wyslijPropozycjeZamiany(4);
			break;
		}
		case '5':
		{
			wyslijPropozycjeZamiany(5);
			break;
		}
		case '6':
		{
			wyslijPropozycjeZamiany(6);
			break;
		}
		case '7':
		{
			wyslijPropozycjeZamiany(7);
			break;
		}
		case '8':
		{
			wyslijPropozycjeZamiany(8);
			break;
		}
		case '9':
		{
			wyslijPropozycjeZamiany(9);
			break;
		}
		case '0':
		{
			wyslijPropozycjeZamiany(0);
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
		case VK_CONTROL:
		{
			CTRLwcisniety = 0;
			break;
		}
		case VK_MENU:
		{
			ALTwcisniety = 0;
			break;
		}
		case 'L':     // zakonczenie zaznaczania metodą lasso
			Lwcisniety = false;
			break;
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
			pMojObiekt->alfa = 0;
			break;
		}
		case VK_RIGHT:
		{
			pMojObiekt->alfa = 0;
			break;
		}

		}

		break;
	}

	} // switch po komunikatach
}


int znajdzGracza(int ostatniaCyfraID) {
	for (int i = 0; i < iLiczbaCudzychOb; i++) {
		if (CudzeObiekty[i]->iID % 10 == ostatniaCyfraID) {
			return CudzeObiekty[i]->iID;
		}
	}
	return -1;
}

void wyslijPropozycjeZamiany(int ostatniaCyfraID) {
	printf("Wyslanie propozycji zamiany do: %d", ostatniaCyfraID);
	Ramka ramka;
	ramka.typ_ramki = OFERTA_TEAM;
	ramka.stan = pMojObiekt->Stan();

	int idOdbiorcyZnalezione = znajdzGracza(ostatniaCyfraID);
	if (idOdbiorcyZnalezione == -1)
		return;

	ramka.iID_adresata = idOdbiorcyZnalezione;
	multi_send->send((char*)&ramka, sizeof(Ramka));
	fclose(f);
}
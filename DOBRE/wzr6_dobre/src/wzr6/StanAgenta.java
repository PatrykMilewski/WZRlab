
import java.io.*;

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

/**
 *
 * @author J
 */
public class StanAgenta implements Serializable {
  public int wiersz,kolumna;          // położenie agenta
  public int ID;                      // identyfikator agenta
  public int ID_druzyny;              // identyfikator druzyny, do ktorej nalezy agent
  public float nagroda;
  public float stan_konta;
  public int liczba_obiektow;         // liczba obiektow zarzadzanych przez agenta
}

import jade.core.Agent;
import jade.core.AID;
import jade.core.behaviours.*;
import jade.lang.acl.*;
import java.util.*;

// Przykładowa klasa zachowania:
class MyOwnBehaviour extends Behaviour
{
  protected MyOwnBehaviour()
  {
  }
  public void action()
  {
  }
  public boolean done() {
    return false;
  }
}

public class BookBuyerAgent extends Agent {

    private String targetBookTitle;    // tytuł kupowanej książki przekazywany poprzez argument wejściowy
    // lista znanych agentów sprzedających książki (w przypadku użycia żółtej księgi - usługi katalogowej, sprzedawcy
    // mogą być dołączani do listy dynamicznie!
    private AID[] sellerAgents = {
      new AID("seller1", AID.ISLOCALNAME),
      new AID("seller2", AID.ISLOCALNAME)};

    // Inicjalizacja klasy agenta:
    protected void setup()
    {

      //doWait(5000);   // Oczekiwanie na uruchomienie agentów sprzedających

      System.out.println("Witam! Agent-kupiec "+getAID().getName()+" (wersja h 2017/18) jest gotów!");

      Object[] args = getArguments();  // lista argumentów wejściowych (tytuł książki)

      if (args != null && args.length > 0)   // jeśli podano tytuł książki
      {
        targetBookTitle = (String) args[0];
        System.out.println("Zamierzam kupić książkę zatytułowaną "+targetBookTitle);

        addBehaviour(new RequestPerformer());  // dodanie głównej klasy zachowań - kod znajduje się poniżej

      }
      else
      {
        // Jeśli nie przekazano poprzez argument tytułu książki, agent kończy działanie:
        System.out.println("Należy podać tytuł książki w argumentach wejściowych kupca!");
        doDelete();
      }
    }
    // Metoda realizująca zakończenie pracy agenta:
    protected void takeDown()
    {
      System.out.println("Agent-kupiec "+getAID().getName()+" kończy istnienie.");
    }

    /**
    Inner class RequestPerformer.
    This is the behaviour used by Book-buyer agents to request seller
    agents the target book.
    */
    private class RequestPerformer extends Behaviour
    {

      private AID bestSeller;     // agent sprzedający z najkorzystniejszą ofertą
      private int bestPrice;      // najlepsza cena
      private Double wantedPrice;
      private int repliesCnt = 0; // liczba odpowiedzi od agentów
      private MessageTemplate mt; // szablon odpowiedzi
      private int step = 0;       // krok

      public void action()
      {
        switch (step) {
        case 0:      // wysłanie oferty kupna
          System.out.print(" Oferta kupna (CFP) jest wysyłana do: ");
          for (int i = 0; i < sellerAgents.length; ++i)
          {
            System.out.print(sellerAgents[i]+ " ");
          }
          System.out.println();

          // Tworzenie wiadomości CFP do wszystkich sprzedawców:
          ACLMessage cfp = new ACLMessage(ACLMessage.CFP);
          for (int i = 0; i < sellerAgents.length; ++i)
          {
            cfp.addReceiver(sellerAgents[i]);          // dodanie adresate
          }
          cfp.setContent(targetBookTitle);             // wpisanie zawartości - tytułu książki
          cfp.setConversationId("book-trade");         // wpisanie specjalnego identyfikatora korespondencji
          cfp.setReplyWith("cfp"+System.currentTimeMillis()); // dodatkowa unikatowa wartość, żeby w razie odpowiedzi zidentyfikować adresatów
          myAgent.send(cfp);                           // wysłanie wiadomości

          // Utworzenie szablonu do odbioru ofert sprzedaży tylko od wskazanych sprzedawców:
          mt = MessageTemplate.and(MessageTemplate.MatchConversationId("book-trade"),
                                   MessageTemplate.MatchInReplyTo(cfp.getReplyWith()));
          step = 1;     // przejście do kolejnego kroku
          break;

        case 1:      // odbiór ofert sprzedaży/odmowy od agentów-sprzedawców
          ACLMessage reply = myAgent.receive(mt);      // odbiór odpowiedzi
          if (reply != null)
          {
            if (reply.getPerformative() == ACLMessage.PROPOSE)   // jeśli wiadomość jest typu PROPOSE
            {
              int price = Integer.parseInt(reply.getContent());  // cena książki
              if (bestSeller == null || price < bestPrice)       // jeśli jest to najlepsza oferta
              {
                bestPrice = price;
                bestSeller = reply.getSender();
                wantedPrice = bestPrice * 0.7;
              }
            }
            repliesCnt++;
            step = 2;// liczba ofert
//            if (repliesCnt >= sellerAgents.length)               // jeśli liczba ofert co najmniej liczbie sprzedawców
//            {
//              step = 2;
//            }
          }
          else
          {
            block();
          }
          break;
        case 2: { // wyslanie propozycji ceny w negocjacji
            System.out.println("Kupiec proponuje "+ wantedPrice +" za "+targetBookTitle+".");
            ACLMessage order = new ACLMessage(ACLMessage.PROPOSE);
            order.addReceiver(bestSeller);
            order.setContent(targetBookTitle + ";" + wantedPrice.toString());
            order.setConversationId("book-trade");
            order.setReplyWith("neg"+System.currentTimeMillis());
            System.out.println(order.toString());
            myAgent.send(order);
            mt = MessageTemplate.and(MessageTemplate.MatchConversationId("book-trade"),
                                     MessageTemplate.MatchInReplyTo(order.getReplyWith()));
            step = 3;
            break;
        }
        case 3: // odbior propozycji ceny
            reply = myAgent.receive(mt);
            if (reply != null)
            {
                if (reply.getPerformative() == ACLMessage.PROPOSE){
                    bestPrice = Integer.parseInt(reply.getContent());
                    if(bestPrice > wantedPrice) {
                        wantedPrice = (bestPrice + wantedPrice) / 2;
                        step = 2;
                    } else {
                        step = 4;
                    }
                } else if(reply.getPerformative() == ACLMessage.REJECT_PROPOSAL){
                    System.out.println("Kupiec odrzucil oferte kupna "+targetBookTitle+".");
                    myAgent.doDelete();
                    step = 999;
                }
            }
            else
            {
              block();
            }

            break;

        case 4:      // wysłanie zamówienia do sprzedawcy, który złożył najlepszą ofertę
          ACLMessage order = new ACLMessage(ACLMessage.ACCEPT_PROPOSAL);
          order.addReceiver(bestSeller);
          order.setContent(targetBookTitle);
          order.setConversationId("book-trade");
          order.setReplyWith("order"+System.currentTimeMillis());
          System.out.println(order.toString());
          myAgent.send(order);
          mt = MessageTemplate.and(MessageTemplate.MatchConversationId("book-trade"),
                                   MessageTemplate.MatchInReplyTo(order.getReplyWith()));
          step = 5;
          break;

        case 5:      // odbiór odpowiedzi na zamównienie
          reply = myAgent.receive(mt);
          if (reply != null)
          {
              if (reply.getPerformative() == ACLMessage.INFORM){
              System.out.println("Tytuł "+targetBookTitle+" został zamówiony.");
              System.out.println("Cena = "+bestPrice);
              myAgent.doDelete();
            }
            step = 999;
          }
          else
          {
            block();
          }
          break;
        }  // switch
      } // action

      public boolean done() {
        return ((step == 2 && bestSeller == null) || step == 999);
      }
    } // Koniec wewnętrznej klasy RequestPerformer
}

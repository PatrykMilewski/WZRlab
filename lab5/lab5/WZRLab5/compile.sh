#!/usr/bin/env bash
javac -cp jade.jar:. src/BookBuyerAgent.java
javac -cp jade.jar:. src/BookSellerAgent.java
java -cp jade.jar:src/ jade.Boot -agents "seller1:BookSellerAgent();seller2:BookSellerAgent();buyer:BookBuyerAgent(Zamek)" -gui

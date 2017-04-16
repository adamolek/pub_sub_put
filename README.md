Projekt na zaliczenie laboratoriów z przedmiotu Nowoczesne Sieci Komputerowe.
Temat projektu: System wymiany komunikatów typu publish/subscribe

Opis ramki wysyłanej do serwera:
bajt 0- określa czy jest to publikacja czy rządanie subskrybcji
bajty 1-16- temat subskrybcji
bajty 17-32- w przypadku publikacji wiadomość
bajt 33- koniec łańcucha('\0', NULL)

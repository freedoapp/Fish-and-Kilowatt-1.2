# Fish-and-Kilowatt-1.2

Monitor consumi elettrici con Fishino

by Mauro Segafredo freedoapp@gmail.com

Progetto vincitore del "Contest Fishino" lanciato dalla rivista Elettronica In

Visita http://blog.elettronicain.it/2016/04/14/monitor-consumi-elettrici-con-fishino-fish-and-kilowatt-by-mauro-segafredo/



Versione 1.0 Compilato con IDE 1.6.8

Originale inviato alla rivista Elettronica In


-> Versione 1.2  18 Ottobre 2016  Compilato con IDE 1.6.11

  -> Nuove Funzionalità: 
  
  -> Aggiunto altri valori nei grafici e csv: Progressivo e Stima consumo orario.  
  
 
  -> Ottimizzazioni: 
  
  -> Aggiunto client.stop per non far bloccare il wifi dopo invio con pushetta 
  
  -> Tolto Serial.begin a riga 324 e Serial.print(c); a riga 540,  
  
  -> Aggiunto else a riga 599 
  
  -> Modificato Formatta_Fname 
  
  -> Commentato Serial.begin(115200);
  
  Grafici highcharts ( http://www.highcharts.com/ )   
  
  Notifica Pushetta ( http://www.pushetta.com/ ) 

/*
 Fish and Kilowatt Versione 1.2  18 Ottobre 2016
 
 http://blog.elettronicain.it/2016/04/14/monitor-consumi-elettrici-con-fishino-fish-and-kilowatt-by-mauro-segafredo/
 http://github.com/freedoapp/Fish-and-Kilowatt
 by Mauro Segafredo freedoapp@gmail.com
 
 
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
 
 
 
 */

#include <Flash.h>
#include <FishinoUdp.h>
#include <FishinoSockBuf.h>
#include <Fishino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

///////////////////////////////////////////////////////////////////////
//           CONFIGURATION DATA -- ADAPT TO YOUR NETWORK             //
//     CONFIGURAZIONE SKETCH -- ADATTARE ALLA PROPRIA RETE WiFi      //

// OPERATION MODE :
// NORMAL (STATION)  -- NEEDS AN ACCESS POINT/ROUTER
// STANDALONE (AP)  -- BUILDS THE WIFI INFRASTRUCTURE ON FISHINO
// COMMENT OR UNCOMMENT FOLLOWING #define DEPENDING ON MODE YOU WANT
// MODO DI OPERAZIONE :
// NORMAL (STATION) -- HA BISOGNO DI UNA RETE WIFI ESISTENTE A CUI CONNETTERSI
// STANDALONE (AP)  -- REALIZZA UNA RETE WIFI SUL FISHINO
// COMMENTARE O DE-COMMENTARE LA #define SEGUENTE A SECONDA DELLA MODALITÀ RICHIESTA
//#define STANDALONE_MODE

// here pur SSID of your network
// inserire qui lo SSID della rete WiFi

 #define MY_SSID  "fish"

// here put PASSWORD of your network. Use "" if none
// inserire qui la PASSWORD della rete WiFi -- Usare "" se la rete non ￨ protetta
 #define MY_PASS "fish"

// comment this line if you want a dynamic IP through DHCP
// obtained IP will be printed on serial port monitor
// commentare la linea seguente per avere un IP dinamico tramite DHCP
// l'IP ottenuto verrà visualizzato sul monitor seriale
// #define IPADDR 192,168,1,251


// define ip address if required
#ifdef IPADDR
IPAddress ip(IPADDR);
#endif


//                    END OF CONFIGURATION DATA                      //
//                       FINE CONFIGURAZIONE                         //
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//                 VARIABILI E FUNZIONI PER WEB SERVER               //

FishinoClient client;
FishinoServer server(80);

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   60

File webFile;               // the web page file on the SD card

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

// searches for the string sfind in the string str
// returns index if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
             //   return 1;
                return index;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}


void SendHeaderOk(){
    client << F("HTTP/1.1 200 OK\n");
}                        

void SendHeaderhtml(){
    client << F("Content-Type: text/html\n");
    client << F("Connection: close\n\n");
}


void SendFile() {
    if (webFile) {
       while(webFile.available()) {
       client.write(webFile.read()); // send web page to client
      }
     webFile.close();
   }
}                        


//              FINE VARIABILI E FUNZIONI PER  WEB SERVER            //
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
// variabili e funzione per la lettura della potenza istantanea      //

// GISoglia indica il valore di soglia differenziale, necessario per rilevare il led superiore acceso.
const int GISoglia = 90;          // Il valore 90 è stato ricavato in maniera sperimentale, eventualmente da modificare.  

// GIContaFlash indica il numero di campionamenti consecutivi con superamento della soglia differenziale, necessari a rilevare un flash valido.
const int GIContaFlash = 150;   //Il valore 150 è stato ricavato in maniera sperimentale, eventualmente da modificare.
 
const int analogInPot = A0;       //ingresso analogico corrispondente al led della potenza attiva
const int analogInReattiva = A1;  //ingresso analogico corrispondente al led della potenza reattiva
const int ledPin =  2;      //uscita digitale per indicatore flash

long GLtoldflash;       //millisecondi rilevati al flash precedente
long GLtflash;          //millisecondi rilevati all'ultimo flash
float GFtinterval = 3000;   //millisecondi intercorsi fra gli ultimi due flash
float GFPotIstantanea;   //Ultima potenza istantanea rilevata


long CheckFlash() {

 int StaLampeggiandoPotenza=0; //contatore di campionamento di flash valido in corso
 
   while ((analogRead(analogInPot)-analogRead(analogInReattiva)) > GISoglia) //fintanto che il valore letto sul led superiore supera del valore di soglia quello letto sul led inferiore ciclo
 {   
  StaLampeggiandoPotenza = StaLampeggiandoPotenza +1; //incremento il contatore di campionamento flash valido
  digitalWrite(ledPin, HIGH);   //accendo il led .....
  
 }
  digitalWrite(ledPin, LOW);    // spengo il led al termine del flash
  if (StaLampeggiandoPotenza > GIContaFlash) { //se il valore soglia è stato superato tante volte quante indicate nella variabile GLContaFlash, ho rilevato un flash valido. 
                                      

    return millis(); //restituisco il valore millis()
  }
  else 
  return 0; //flash non rilevao, restituisco 0
  }
      

// fine variabili e funzione per la lettura della potenza istantanea //
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//                      VARIABILI PER NOTIFICHE                      //

// l'ultima volta che vi siete connessi al server, in millisecondi
long lastConnectionTime;

// ritardo tra gli aggiornamenti, in millisecondi
const unsigned long postingInterval = 60L * 1000L;


// GLLimite indica il limite in watt oltre il quale viene inviata la notifica tramite Pushetta.
int GLLimite = 4000;      //valore predefinito 4kW.


boolean GBPrimaNotifica = true; //serve a indicare che non è stata ancora inviata nessuna notifica tramite Pushetta 

#ifndef STANDALONE_MODE

  void sendToPushetta(){
                 client.stop();
                 if (client.connect("api.pushetta.com", 80)) 
                 {                                     
                   client << F("POST /api/pushes/") << F("Controlla_KW"); // inserire il proprio channel
                   client << F("/ HTTP/1.1\n");
                   client << F("Host: api.pushetta.com\n");
                   client << F("Authorization: Token ") << F("1234567890123456789012345678901234567890\n"); // da inserire il proprio API Key
                   client << F("Content-Type: application/json\n"); 
                   client << F("Content-Length: 74\n\n");  //  74 = 26 + 2 + 46 la lunghezza del messaggio è data dalla lunghezza della descrizione
                   client << F("{ \"body\" : \"");
                   client << F("Attenzione soglia superata");
                   client << F("\", \"message_type\" : \"text/plain\" }\n\n");
                } 

     client.stop(); // close the connection
}

#endif


//            FINE VARIABILI E FUNZIONI PER  NOTIFICHE               //
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// VARIABILI E FUNZIONI PER GESTIONE ORARIO E LOGFILE                //
 

byte GLwm;  //Consumo in WATT del minuto corrente
int GLwh;  //Consumo orario in WATT 
long GLwg; //Consumo giornaliero in WATT

byte GLs; //secondo corrente
byte GLm; //minuto corrente
byte GLmOld; //minuto precendete lettura
byte GLh; //ora corrente
byte GLhOld; //ora precendete lettura
byte GLg; //giorno corrente
byte GLgOld; //giorno precendete lettura
byte GLmese; //mese corrente
byte GLanno; //anno corrente
byte GLgs; // giorno della settimana 1--7

char GLFname_m[] = "aamm.csv";      //File di log mensile. Ad ex. 1604.csv per il mese di aprile 2016
char GLFname_g[] = "aammgg.csv";    //File di log giornaliero. Ad ex. 160425 per il log del 25 aprile 2016
char GLFname_h[] = "aammgghh.csv";  //File di log orario. Ad ex. 16042513 per il log dei consumi dalle 13:00 alle 13:59 del 25 aprile 2016

void Formatta_Fname()
{
    GLFname_m[0] = GLanno/10 + '0';
    GLFname_g[0] = GLanno/10 + '0';
    GLFname_h[0] = GLanno/10 + '0';
    GLFname_m[1] = GLanno%10 + '0';
    GLFname_g[1] = GLanno%10 + '0';
    GLFname_h[1] = GLanno%10 + '0';

    GLFname_m[2] = GLmese/10 + '0';
    GLFname_g[2] = GLmese/10 + '0';
    GLFname_h[2] = GLmese/10 + '0';
    GLFname_m[3] = GLmese%10 + '0';
    GLFname_g[3] = GLmese%10 + '0';
    GLFname_h[3] = GLmese%10 + '0';

   // GLFname_g[4] = GLgOld/10 + '0';
   // GLFname_h[4] = GLgOld/10 + '0';
   // GLFname_g[5] = GLgOld%10 + '0';
   // GLFname_h[5] = GLgOld%10 + '0';

   // GLFname_h[6] = GLhOld/10 + '0';
   // GLFname_h[7] = GLhOld%10 + '0';

    GLFname_g[4] = GLg/10 + '0';
    GLFname_h[4] = GLg/10 + '0';
    GLFname_g[5] = GLg%10 + '0';
    GLFname_h[5] = GLg%10 + '0';

    GLFname_h[6] = GLh/10 + '0';
    GLFname_h[7] = GLh%10 + '0';

}

#define DS1307_ADDRESS 0x68 // Indirizzo DS1307 
                            
// Converte dalla notazione BCD alla notazione in base 10
byte bcdToDec(byte val)  
{
  // val % 16 => corrisponde ai 4 bit a destra di un byte
  // (val - val % 16)/ 16 => corrisponde ai 4 bit a sinistra di un byte
  // ovvero a shiftare verso destra di 4 bit quindi 
  // (val-val%16)/16 equivale a val >> 4
  return ( (val-val%16)/16*10 + val%16 );
}

boolean getDateTime()
{
  
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();

   Wire.requestFrom(DS1307_ADDRESS, 7);
  if (Wire.available())
  {
      GLs = bcdToDec(Wire.read());
      GLm = bcdToDec(Wire.read());
      GLh = bcdToDec(Wire.read() & 0b111111); // modo 24 ore
                                                  // considero i primi 6 bit
      GLgs = Wire.read(); // non mi serve convertire (Range da 1 a 7 => 3 bit) 
      GLg = bcdToDec(Wire.read());
      GLmese = bcdToDec(Wire.read());
      GLanno = bcdToDec(Wire.read());
     return true; 
    }
   return false;
}



//   FINE VARIABILI E FUNZIONI PER GESTIONE ORARIO E LOGFILE         //
///////////////////////////////////////////////////////////////////////

void setup()
{
  //Initialize serial and wait for port to open:
  // apre la porta seriale e ne attende l'apertura
  // Serial.begin(115200);

  // wait for serial port to connect. Needed for Leonardo only
  // attende l'apertura della porta seriale. Necessario solo per le boards Leonardo
  // while (!Serial)
  //   ;

  // initialize SPI
  // inizializza il modulo SPI
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2);

  // reset and test wifi module
  // resetta e testa il modulo WiFi
  // Serial << F("Resetting Fishino...");
  while(!Fishino.reset())
  {
  //   Serial << ".";
    delay(500);
  }

  // set PHY mode to 11G
  Fishino.setPhyMode(PHY_MODE_11G);
  
  // for AP MODE, setup the AP parameters
#ifdef STANDALONE_MODE
  // setup SOFT AP mode
  // imposta la modalitè SOFTAP
  // Serial << F("Setting mode SOFTAP_MODE\r\n");
  Fishino.setMode(SOFTAP_MODE);

  // stop AP DHCP server
  // Serial << F("Stopping DHCP server\r\n");
  Fishino.softApStopDHCPServer();
  
  // setup access point parameters
  // imposta i parametri dell'access point
  // Serial << F("Setting AP IP info\r\n");
  Fishino.setApIPInfo(ip, ip, IPAddress(255, 255, 255, 0));

  //  Serial << F("Setting AP WiFi parameters\r\n");
  Fishino.softApConfig(MY_SSID, MY_PASS, 1, false);
  
  // restart DHCP server
  //  Serial << F("Starting DHCP server\r\n");
  Fishino.softApStartDHCPServer();
  
#else
  // setup STATION mode
  // imposta la modalitè STATION
  //  Serial << F("Setting mode STATION_MODE\r\n");
  Fishino.setMode(STATION_MODE);

  // NOTE : INSERT HERE YOUR WIFI CONNECTION PARAMETERS !!!!!!
  //  Serial << F("Connecting to AP...");
  while(!Fishino.begin(MY_SSID, MY_PASS))
  {
 //   Serial << ".";
    delay(500);
  }
 // Serial << F("OK\r\n");

  // setup IP or start DHCP server
#ifdef IPADDR
  Fishino.config(ip);
#else
  Fishino.staStartDHCP();
#endif

  // wait for connection completion  
  //  Serial << F("Waiting for IP...");
  while(Fishino.status() != STATION_GOT_IP)
  {
 //   Serial << ".";
    delay(500);
  }
 //  Serial << F("OK\r\n");

#endif

  //  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(4)) {
  //   Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  // Serial.println("card initialized.");

  // print your Fishino's IP address:
  // stampa l'indirizzo IP del Fishino
  // IPAddress ip = Fishino.localIP();
  //  Serial.print("IP Address: ");
  // Serial.println(ip);

  Wire.begin(); // Si connette al bus i2c

 //Leggo data e ora
 if (  getDateTime()) {

    GLmOld = GLm;
    GLhOld = GLh;
    GLgOld = GLg;
  
    Formatta_Fname();

 }

   
  // start listening for clients
  // inizia l'attesa delle connessioni
  server.begin();

}


void loop()
{


////////////////////////////////////////
//     Variabili di utilità           //
byte i;
byte indexFname;
char LFname_m[] = "00000000.csv";
File dataFile;


// Verifico cambio di minuto, ora o giorno  

if (  getDateTime()) {
    if ( GLm != GLmOld ) {
  
  // scaduto un minuto, scrivi su file GLFname_h
  // il numero di watt (GLwm ) consumati nell'ultimo minuto ( GLmOld )
  // Vers 1.2 e il numero di watt al minuto moltiplicato 60 per avere la proiezione in w/h
    
    dataFile = SD.open(GLFname_h, FILE_WRITE);
    if (dataFile) {
      
      dataFile.print(GLmOld);
      dataFile.print(",");
      dataFile.print(GLwm);
      dataFile.print(",");       // Vers 1.2
      dataFile.print(GLwm*60);   // Vers 1.2
      dataFile.print(",");       // Vers 1.2
      dataFile.println(GLwh);    // Vers 1.2
      dataFile.close();
    }

  // azzero i watt consumati nel minuto  
    GLwm = 0;
    GLmOld = GLm;
    }
   
    if ( GLh != GLhOld ) {

  
  // scaduta un'ora, scrivi su file GLFname_g
  // il numero di watt (GLwh ) consumati nell'ultima ora ( GLhOld )
    
    dataFile = SD.open(GLFname_g, FILE_WRITE);
    if (dataFile) {
      
      dataFile.print(GLhOld);
      dataFile.print(",");
      dataFile.print(GLwh);
      dataFile.print(",");          // Vers 1.2
      dataFile.println(GLwg);       // Vers 1.2
      dataFile.close();
    }
    
  // rigenero i nomi dei 3 file di log    
     Formatta_Fname();
     
  // azzero i watt consumati nell'ora
 
     GLwh = 0;
     GLhOld = GLh;
    }
  
    if ( GLg != GLgOld ) {
   
  // scaduto un giorno, scrivi su file GLFname_m
  // il numero di watt (GLwg ) consumati nel giorno ( GLgOld )
     
    dataFile = SD.open(GLFname_m, FILE_WRITE);
    if (dataFile) {
      
      dataFile.print(GLgOld);
      dataFile.print(",");
      dataFile.println(GLwg);
      dataFile.close();
    
    }
   
  // rigenero i nomi dei 3 file di log 
       Formatta_Fname();
     
  // azzero i watt consumati nell'ora 
       GLwg = 0;
     GLgOld = GLg;
  
    }
  }
  
  
 // campiono il led della potenza attiva
 GLtflash = CheckFlash();

 // se rilevato il flash, procedo con il calcolo della potenza istantanea
 if ( GLtflash > 0 )
 {
   GFtinterval = (GLtflash - GLtoldflash) ;
   GLtoldflash = GLtflash;
   GFPotIstantanea = 3600/GFtinterval*1000; //Calcolo la potenza istantanea
   
   //ho consumato un watt, aggiorno i contatori di consumo
   GLwm++;  
   GLwh++;  
   GLwg++;

}
 
  // gestisco le chiamate web
  // wait for a new client:
  // attende nuovi clienti

  client = server.available();

 
    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
             //   Serial.print(c);
                // limit the size of the stored received HTTP request
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                   
                    indexFname=StrContains(HTTP_req, "FileName=");
                    
                     if (indexFname>0) {
                          SendHeaderOk();
                          client << F("Content-Type: text/csv\n");
                          client << F("Connection: keep-alive\n\n");
                      
                          for ( i = 0; i < 12; i++) {
                           if ( HTTP_req[i+indexFname+2] == 'H' ) {
                        
                            LFname_m[i] = '\0';
                            
                            if ( i == 8 ) {
                             client << F("Giorno,Watt\r\n");
                            } else
                            if ( i == 10 ) {
                             client << F("Ora,Watt,WattPrg\r\n"); // Vers. 1.2
                            } 
                          
                           break;
                            }
                           LFname_m[i] = HTTP_req[i+indexFname+1];
                           if ( i == 11 ){
                             client << F("Minuto,Watt/m,Watt/h,WattPrg\r\n"); // Vers. 1.2
                            } 
                          } 
                         
                      webFile = SD.open(LFname_m);        // open web page file
                      SendFile();
                   }
                         
                    else      
                  
                    if (StrContains(HTTP_req, "leggivalore")>0) {
                      SendHeaderOk();
                      client << F("Content-Type: text/json\n\n");
                      client << F("[") << millis()  << F(",") << GFPotIstantanea << F("]\n");
                    }
                    
                    else 
                     if (StrContains(HTTP_req, "liveview")>0) {
                          SendHeaderOk();
                          SendHeaderhtml();
                          webFile = SD.open("liveview.htm");        // open web page file
                          SendFile();
                      
                    }
                    else
                    if (StrContains(HTTP_req, "chfile")>0) {
                          SendHeaderOk();
                          SendHeaderhtml();
                          webFile = SD.open("chfile.htm");        // open web page file
                          SendFile();
                      
                    }
                    else
                    if (StrContains(HTTP_req, "logfile.htm")>0) {
                           
                          SendHeaderOk();
                          SendHeaderhtml();
                          webFile = SD.open("logfile.htm");        // open web page file
                          SendFile();
                      
                    }   
                    else
                    if ((StrContains(HTTP_req, "GET / ") >0 )
                            || (StrContains(HTTP_req, "GET /index.htm")>0)) {
                           
                            SendHeaderOk();
                            SendHeaderhtml();
                            client << F("<!DOCTYPE HTML>\n");
                            client << F("<html> <HEAD> <meta http-equiv=refresh content=5 /> <TITLE>Fish and kiloWatt</TITLE> </HEAD>\n");
                          if (GFPotIstantanea > GLLimite) 
                           {             
                             client << F("<body bgcolor=\"#ff0000\">\n");
                           }  
                             client << F("<H1><p align=\"center\">Fish and Kilowatt<BR>\n");
                             client << F(" Potenza Istantanea: ") << GFPotIstantanea;
                             client << F(" Wh</H1></p><H2><p align=\"center\">");
                        #ifndef STANDALONE_MODE
                             client << F("<a  href=\"liveview.htm\">Live View</a><BR>");
                             client << F("<a  href=\"chfile.htm\">Grafici Storici</a><BR>");
                        #endif
                             client << F("<a  href=\"logfile.htm\">File di Log</a>");

                             client << F("</H2></p></html>");
                    }
                  
                    // display received HTTP request on serial port
                    //Serial.print(HTTP_req);
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
               
                    for (byte i = 0; i < REQ_BUF_SZ; i++) {
                     HTTP_req[i] = 0;
                    }
                    for (byte i = 0; i < 13; i++) {
                     LFname_m[i] = 0;
                    }
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
     }// end if (client)

  // verifico il superamento della soglia predifinita
   if (GFPotIstantanea > GLLimite) 
    { 
    
      if ( ( (millis() - lastConnectionTime) > postingInterval) || GBPrimaNotifica )
      // se ho rilevato il primo superamento della soglia, oppure se è passato più di un minuto
      // dall'ultimo superamento della soglia, attivo la notifica
     { 
    
    #ifndef STANDALONE_MODE

        sendToPushetta();      
    
    #endif

      // registra il tempo in cui è stata fatta la connessione
       lastConnectionTime = millis();
       GBPrimaNotifica = false;
      }   
    
   }
       
}

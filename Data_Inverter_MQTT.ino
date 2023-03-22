/*
  Autor: DromegaWeb
  title: interface between Voltronic basic inverters or compatible with Home Assistant via MQTT
  Hardware: Arduino mega with Ethernet shield
  Date release : 06-03-2023
  Version: 1.01
*/

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

#define DEBUG 1 // Debug output to serial console


IPAddress deviceIp(192, 168, 2, 99);                                     // Device settings
byte deviceMac[] = { 0xAB, 0xCD, 0xFE, 0xFE, 0xFE, 0xFE };
char* deviceId  = "inverter"; // Name of the sensor
char *stateTopic[] =                                                     //  MQTT messages
{
        "ha/inv/voltage_linein",        //   0
        "ha/inv/f_hz_line_in",          //   1
        "ha/inv/v_out_inverter",        //   2 
        "ha/inv/f_hz_line_out",         //   3
        "ha/inv/apparent_power",        //   4
        "ha/inv/active_power",          //   5
        "ha/inv/output_power_p",        //   6
        "ha/inv/bus_voltage",           //   7
        "ha/inv/battery_volt",          //   8
        "ha/inv/bat_charge_cur",        //   9
        "ha/inv/bat_capacity",          //   10
        "ha/inv/inv_temp",              //   11
        "ha/inv/pv_current",            //   12
        "ha/inv/pv_voltage",            //   13
        "ha/inv/bat_v_scc",             //   14
        "ha/inv/bat_dis_current",       //   15
        "ha/inv/pv_w",                  //   16
        "ha/inv/stat_01",               //   17
        "ha/inv/stat_02",               //   18
        "ha/inv/stat_03",               //   19
        "ha/inv/stat_04",               //   20
        "ha/inv/stat_05",               //   21
        "ha/inv/stat_06",               //   22
        "ha/inv/stat_07",               //   23
        "ha/inv/stat_08"                //   24
}  ;                                      // MQTT topic where values are published

/*
const String mainMenuItems[] = {"Item1", "Item2"};                            //Menu level 0
const String modeMenuItems[] = {"Item3", "Item4", "Item5"};                   //menu level 1
const String settingsMenuItems[] = {"Item6", "Item7", "Item 8", "Item 9"}     //menu level 2
const String allMenus[][3] = {mainMenuItems, modeMenuItems, settingsMenuItems};
 */

char x[8];
char buf[6]; // Buffer to store the sensor value
int updateInterval = 20000; // Interval in milliseconds

// MQTT server settings
IPAddress mqttServer(192, 168, 2, 101);
int mqttPort = 1883;

EthernetClient ethClient;
PubSubClient client(ethClient);




int inByte_Read;
int inByte_Inveter;
char info_inveter[120];
byte count_inv = 0;
int dati_inverter_int[120];
//char aaa;
//int iii;
//float aaaa;
float valori_inverter[20];               
unsigned long currentMillis;
unsigned long previousMillis = 0;         // will store last updated
unsigned long interval = 60000;           // interval of reading (milliseconds)

/*----------------------------------------------------------------------------*/

void reconnect() {
  while (!client.connected()) {
#if DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
    if (client.connect(deviceId, "mqtt_bf", "bfmqttpass")) {
#if DEBUG
      Serial.println("connected");
#endif
    } else {
#if DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      delay(5000);
    }
  }
}


/*----------------------------------------------------------------------------*/



void setup() 
{
#if DEBUG
  Serial.begin(57600);                      //      x debug   
#endif
  Serial1.begin(2400);                      //      Serial1.begin(2400, SERIAL_8N1);
  client.setServer(mqttServer, mqttPort);   //      MQTT server setting
  Ethernet.begin(deviceMac, deviceIp);      //      Ethernet initialized
  delay(2500);  


}

void loop() {
  
  if (!client.connected()) 
      {
      reconnect();
      }
  client.loop();
  currentMillis = millis();
  
  if (currentMillis - previousMillis > interval)        // richiesta infomazioni inverter con un periodo definito in "interval"
      {
      previousMillis = currentMillis;
      get_inveter_QPIGS();                              // Richiesta valori ad inverter
      count_inv = 0 ;
      }
      
  if (Serial1.available() > 0)                          // lettura dati da inverter dopo l'interrogazione
     { 
      do {
        if (Serial1.available()){
            inByte_Inveter = Serial1.read();                            //ricevi come byte
#if DEBUG            
            Serial.write(inByte_Inveter);                               // x debug   
#endif
            dati_inverter_int[count_inv] = Converter_Int(inByte_Inveter);
            count_inv++; 
          } 
        }while(inByte_Inveter != 0x0D );                // Continua a leggere la seriale inverter finche non trova <cr>
     }
     
  if (inByte_Inveter == 0x0D )  
     {
      inByte_Inveter = 0;
      Inverter_Value();                                 // converti dati seriale in parametri inverter
#if DEBUG
      VisulaizzaValori(0);      // DEBUG - conversione dati in lingia (IT , EN) e visualizzazione su monotor seriale
#endif                          //       - data conversion in language (IT , EN) and visualization on serial monitor
      publish_mqtt_inv();
     }
 
}

void publish_mqtt_inv()
{   
  for (int inv_par = 0; inv_par < 25; inv_par++)  
      {
       dtostrf(valori_inverter[inv_par],5,1,x);
       client.publish(stateTopic[inv_par], x);        // MQTT trasmissione dati -- MQTT data transmission
#if DEBUG
       Serial.print(inv_par);
       Serial.print(" Topic sended  ");     
       Serial.println(stateTopic[inv_par]);    
#endif
       }
}

/* ---------------------------------------------------------------------------------------------------------------
 * --------  S C A R I C O   D A  I N V E R T E R  --  D O W N  L O A D   F  R O M   I  N V E R T E R  ----------- 
 * ---------------------------------------------------------------------------------------------------------------
Command: QPIGS <CRC><cr>
data:      0     1    2     3    4    5   6   7    8    9  10   11   12   13    14    15   byte  16
Device: (BBB.B CC.C DDD.D EE.E FFFF GGGG HHH III JJ.JJ KKK OOO TTTT EEEE UUU.U WW.WW PPPPP 76543210<CRC><cr>
example:(237.1 49.9 229.8 49.9 0965 0807 017 362 52.00 004 042 0027 03.3 302.3 00.00 00000 00010110 00 00 01003 011P⸮
 *      012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
 *                1         2         3         4         5         6         7         8         9         0         1
 *                                                                                                          1         1 
 *
 */

void VisulaizzaValori(int Lingua)  {
      Serial.println(); 
     if (Lingua == 0) {
          Serial.print("Tensione di Rete           Vac  ");   Serial.println(valori_inverter[0],1);                     // x debug  valore di Grid voltage
          Serial.print("Frequenza di Rete           Hz   ");  Serial.println(valori_inverter[1]);                    // x debug  valore di Grid frequency
          Serial.print("Tensione uscita Inverter   Vac  ");   Serial.println(valori_inverter[2]);                 // x debug  valore di AC output voltage
          Serial.print("Frequenza uscita Inverter   Hz   ");  Serial.println(valori_inverter[3]);               // x debug  valore di AC output frequency
          Serial.print("Potenza Apparente          VAT  ");   Serial.println(valori_inverter[4]);                    // x debug  valore di AC output apparent power
          Serial.print("Potenza Attiva             WAT  ");   Serial.println(valori_inverter[5]); Serial.println();      // x debug  valore di AC output active power
          Serial.print("Carico Inverter              %    ");  Serial.println(valori_inverter[6]);               // x debug  valore di Output load percent
          Serial.print("Tensione di BUS inverter   Vdc  ");   Serial.println(valori_inverter[7]);                       // x debug   BUS voltage
          Serial.print("Tensione batterie          Vdc   ");  Serial.println(valori_inverter[8]);                   // x debug   Battery voltage
          Serial.print("Corrente di carica batterie  A    "); Serial.println(valori_inverter[9]);                   // x debug   Battery charging current
          Serial.print("Capacita residua batteria    %   ");   Serial.println(valori_inverter[10]);                   // x debug   Battery capacity
          Serial.print("Temperatura Inverter        °C   ");   Serial.println(valori_inverter[11]);                   // x debug   Inverter heat sink temperature
          Serial.print("Tensione della batteria da SCC   Vdc  ");  Serial.println(valori_inverter[14]);                   // x debug   Battery voltage from SCC
          Serial.print("Corrente di scarica della batteria A  ");  Serial.println(valori_inverter[15]);Serial.println();       // x debug   Battery voltage from SCC
          Serial.print("Corrente Fotovoltaico        A    ");  Serial.println(valori_inverter[12]);                   // x debug   PV Input current for battery
          Serial.print("Tensione Fotovoltaico      Vdc  ");  Serial.println(valori_inverter[13]);                   // x debug   PV Input voltage
          Serial.print("Potenza fotovoltaico      Watt  ");  Serial.println(valori_inverter[16]);
          Serial.println(); 
          Serial.print("add SBU priority version,   1:yes       0:no          = ");  Serial.println(dati_inverter_int[83]);
          Serial.print("configuration status:       1: Change   0: unchanged  = ");  Serial.println(dati_inverter_int[84]);
          Serial.print("SCC firmware version        1: Updated  0: unchanged  = ");  Serial.println(dati_inverter_int[85]);
          Serial.print("Load status:                1:Load on   0: Load off   = ");  Serial.println(dati_inverter_int[86]);
          Serial.print("tensione stabilizzazione durante la ricarica della batteria  ");  Serial.println(dati_inverter_int[87]);
          Serial.print("Charging status( Charging on/off)            ");  Serial.println(dati_inverter_int[88]);
          Serial.print("Charging status( SCC charging on/off)        ");  Serial.println(dati_inverter_int[89]);
          Serial.print("Charging status( AC charging on/off)         ");  Serial.println(dati_inverter_int[90]);
     }
     if (Lingua == 1) {
          Serial.print("Grid voltage             Vac ");  Serial.println(valori_inverter[0]);                     // x debug  valore di Grid voltage
          Serial.print("Grid frequency           Hz  ");  Serial.println(valori_inverter[1]);                    // x debug  valore di Grid frequency
          Serial.print("AC output voltage        Vac ");  Serial.println(valori_inverter[2]);                 // x debug  valore di AC output voltage
          Serial.print("AC output frequency      Hz  ");  Serial.println(valori_inverter[3]);               // x debug  valore di AC output frequency
          Serial.print("apparent power           VAT ");  Serial.println(valori_inverter[4]);                    // x debug  valore di AC output apparent power
          Serial.print("active power             WAT ");  Serial.println(valori_inverter[5]);                      // x debug  valore di AC output active power
          Serial.print("Output load percent      %   ");  Serial.println(valori_inverter[6]);               // x debug  valore di Output load percent
          Serial.print("BUS voltage              Vdc ");  Serial.println(valori_inverter[7]);                       // x debug   BUS voltage
          Serial.print("Battery voltage          Vdc ");  Serial.println(valori_inverter[8]);                   // x debug   Battery voltage
          Serial.print("Battery charging current A   ");  Serial.println(valori_inverter[9]);                   // x debug   Battery charging current
          Serial.print("Battery capacity         %   ");  Serial.println(valori_inverter[10]);                   // x debug   Battery capacity
          Serial.print("Inverter temperature     °C  ");  Serial.println(valori_inverter[11]);                   // x debug   Inverter heat sink temperature
          Serial.print("PV current for battery   A   ");  Serial.println(valori_inverter[12]);                   // x debug   PV Input current for battery
          Serial.print("PV Input voltage         Vdc ");  Serial.println(valori_inverter[13]);                   // x debug   PV Input voltage
          Serial.print("Battery voltage from SCC Vdc ");  Serial.println(valori_inverter[14]);                   // x debug   Battery voltage from SCC
          Serial.print("Battery discharge current A  ");  Serial.println(valori_inverter[15]);                   // x debug   Battery voltage from SCC
          Serial.print("Power PV               Watt  ");  Serial.println(valori_inverter[16]);
          Serial.println(); 
          Serial.print("add SBU priority version, 1:yes,0:no         ");  Serial.println(dati_inverter_int[83]);
          Serial.print("configuration status: 1: Change 0: unchanged ");  Serial.println(dati_inverter_int[84]);
          Serial.print("SCC firmware version 1: Updated 0: unchanged ");  Serial.println(dati_inverter_int[85]);
          Serial.print("Load status: 0: Load off 1:Load on           ");  Serial.println(dati_inverter_int[86]);
          Serial.print("battery voltage to steady while charging     ");  Serial.println(dati_inverter_int[87]);
          Serial.print("Charging status( Charging on/off)            ");  Serial.println(dati_inverter_int[88]);
          Serial.print("Charging status( SCC charging on/off)        ");  Serial.println(dati_inverter_int[89]);
          Serial.print("Charging status(AC charging on/off)          ");  Serial.println(dati_inverter_int[90]);
     }
     Serial.println("--------------------------------------------------------------------------------------------------");                      // x debug   a capo sul monitor seriale dopo la ricezione dei dati dall'inverter
 
  
}

void Inverter_Value(){
      valori_inverter[0] =  dati_inverter_int[1]*100  +
                            dati_inverter_int[2]*10   +
                            dati_inverter_int[3]      +
                            dati_inverter_int[5]*0.1  ;
      valori_inverter[1] =  dati_inverter_int[7]*10   +
                            dati_inverter_int[8]      +
                            dati_inverter_int[10]*0.1 ;
      valori_inverter[2] =  dati_inverter_int[12]*100 +
                            dati_inverter_int[13]*10  +
                            dati_inverter_int[14]     +
                            dati_inverter_int[16]*0.1 ;
      valori_inverter[3] =  dati_inverter_int[18]*10  +
                            dati_inverter_int[19]     +
                            dati_inverter_int[21]*0.1;
      valori_inverter[4] =  dati_inverter_int[23]*1000+
                            dati_inverter_int[24]*100 +
                            dati_inverter_int[25]*10  +
                            dati_inverter_int[26]     ;
      valori_inverter[5] =  dati_inverter_int[28]*1000+
                            dati_inverter_int[29]*100 +
                            dati_inverter_int[30]*10  +
                            dati_inverter_int[31]     ;
      valori_inverter[6] =  dati_inverter_int[33]*100 +
                            dati_inverter_int[34]*10  +
                            dati_inverter_int[35]     ;
      valori_inverter[7] =  dati_inverter_int[37]*100 +
                            dati_inverter_int[38]*10  +
                            dati_inverter_int[39]     ;
      valori_inverter[8] =  dati_inverter_int[41]*10  +
                            dati_inverter_int[42]     +
                            dati_inverter_int[44]*0.1 +
                            dati_inverter_int[45]*0.01;
      valori_inverter[9] =  dati_inverter_int[47]*100 +
                            dati_inverter_int[48]*10  +
                            dati_inverter_int[49]     ;
      valori_inverter[10] = dati_inverter_int[51]*100 +
                            dati_inverter_int[52]*10  +
                            dati_inverter_int[53]     ;
      valori_inverter[11] = dati_inverter_int[55]*1000+
                            dati_inverter_int[56]*100 +
                            dati_inverter_int[57]*10  +
                            dati_inverter_int[58]     ;
      valori_inverter[12] = dati_inverter_int[60]*10  +
                            dati_inverter_int[61]     +
                            dati_inverter_int[63]*0.1 ;
      valori_inverter[13] = dati_inverter_int[65]*100 +
                            dati_inverter_int[66]*10  +
                            dati_inverter_int[67]     +
                            dati_inverter_int[69]*0.1 ;
      valori_inverter[14] = dati_inverter_int[71]*10  +
                            dati_inverter_int[72]     +
                            dati_inverter_int[74]*0.1 +
                            dati_inverter_int[75]*0.01;
      valori_inverter[15] = dati_inverter_int[77]*10000+
                            dati_inverter_int[78]*1000 +
                            dati_inverter_int[79]*100  +
                            dati_inverter_int[80]*10   +
                            dati_inverter_int[81]      ;
      valori_inverter[16] = dati_inverter_int[99]*1000 +
                            dati_inverter_int[100]*100 +
                            dati_inverter_int[101]*10  +
                            dati_inverter_int[102]     ;                            
}

int Converter_Int(int DataReceived ) {
  int Num_intero;
  switch (DataReceived) {
      case 48:
        Num_intero = 0;
        break;
      case 49:
        Num_intero = 1;
        break;
      case 50:
        Num_intero = 2;
        break;
      case 51:
        Num_intero = 3;
        break;
      case 52:
        Num_intero = 4;
        break;
      case 53:
        Num_intero = 5;
        break;
      case 54:          
        Num_intero = 6;
        break;
      case 55:
        Num_intero = 7;
        break;
      case 56:
        Num_intero = 8;
        break;
      case 57:
        Num_intero = 9;
        break;
  }
      return Num_intero;
}



void get_inveter_QPIGS() {            // Scarico Parametri QPIGS
    Serial1.write(char(0x51));
    Serial1.write(char(0x50));
    Serial1.write(char(0x49));
    Serial1.write(char(0x47));
    Serial1.write(char(0x53));
    Serial1.write(char(0xB7)); //CRC
    Serial1.write(char(0xA9)); //CRC
    Serial1.write(char(0x0D)); //cr
    }

void get_inveter_QPIWS() {            // Lettura segnalazionie allarmi
    Serial.write("Mando QPIWS "); 
    Serial1.write(char(0x51));
    Serial1.write(char(0x50));
    Serial1.write(char(0x49));
    Serial1.write(char(0x57));
    Serial1.write(char(0x53));
    Serial1.write(char(0xB4)); //CRC
    Serial1.write(char(0xDA)); //CRC
    Serial1.write(char(0x0D)); //cr
    }

void get_inveter_QPIRI() {            // Scarico Parametri QPIRI
    Serial1.write(char(0x51));
    Serial1.write(char(0x50));
    Serial1.write(char(0x49));
    Serial1.write(char(0x52));
    Serial1.write(char(0x49));
    Serial1.write(char(0xF8)); //CRC
    Serial1.write(char(0x54)); //CRC
    Serial1.write(char(0x0D)); //cr
    }

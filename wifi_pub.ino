void commandcheck(){
  if (commandflag) {                     //sent smbus command by subscribe("***/pmbus/set")
      if(smbus_data[0] == 0xAA){                                                   // set pmbus 
         if     (smbus_data[1] == 0) ps_i2c_address = smbus_data[2];           //[AA 00 XX] Modify the Pmbus device address
         else if(smbus_data[1] == 1) lgInterval = ( smbus_data[2]<<8 )  + smbus_data[3];  //[AA 01 XX XX] Set pmbus poll time /ms;
         else if(smbus_data[1] == 2) pmbusflagset(smbus_data[2]);         //[AA 02 00] Disable PMbus.   
         else if(smbus_data[1] == 3) setpecstatus(smbus_data[2]);         //[AA 03 01/00] PEC Enable/Disable.
         else if(smbus_data[1] == 0xA0) set_custom(smbus_data[2]);    //set WiFI MQTT broker from EEPROM
         else if(smbus_data[1] == 0xBB) esprestar();                 //reset device
       }
       commandflag = false;
      //  buttonflag = true;
  }
}

void rtcUnixtime(){
    unsigned long currentMillis = millis();
    if(currentMillis - rtcMillis >= 1000){
       rtcMillis = currentMillis;
       ld.unixtime++;
    }

}

void checkSensors(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= lgInterval){
        previousMillis = currentMillis;
        ledflash();
        ld.tempLm73 =  get_lm73_temp();
        ld.tempKa = thermocouple_a.readCelsius();
        ld.tempKb = thermocouple_b.readCelsius();
        Serial.printf("LM73_temp_sensor: %3.2f, \n", ld.tempLm73);
        Serial.printf("Ka_temp_sensor: %3.2f, \n", ld.tempKa);
        Serial.printf("Kb_temp_sensor: %3.2f, \n", ld.tempKb);
        count++;
        publishLog();
        // oled_display();
        displaydatatime();
        // Serial.println(ntpClient.getUnixTime());
        // buttonflag = true;                  
     } 
}

void setWifiMqtt(){
  int k = 0; 
  Log.notice(CR );
  Log.notice(F("ESP8266 Device ID topic: %s." CR), DEVICE_ID_Topic);
  delay(10);
  eeprom_read_setup();
  Log.noticeln("Connecting to %s", eep.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(eep.ssid, eep.password);
  while (WiFi.status() != WL_CONNECTED) {
    wifistatus = true;
    delay(500);
    k++;
    ledflash();
    Log.notice(".");
    if( k >= 30){
        wifistatus = false;
        break;
     }
  }   
      // randomSeed(micros());
      if(wifistatus){
      Log.noticeln("WiFi Connected.");
      Log.noticeln("IP address: %s", WiFi.localIP().toString().c_str());
      u8g2.clearBuffer();				
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0,10,"WiFi Connected");	
      // u8g2.drawStr(0,20,"IP:");
      u8g2.drawStr(0,20,WiFi.localIP().toString().c_str());
      u8g2.sendBuffer();					
      client.setCallback(callback);
      String client_id;
      client_id = clientID + String(WiFi.macAddress());
      Log.noticeln("Client_id = %s", client_id.c_str());
      client.setServer(eep.mqtt_broker, mqtt_port);
      delay(100);
      if(client.connect(client_id.c_str(), mqtt_user, mqtt_password)) {
          mqttflag = true;
          Log.noticeln("MQTT Broker Connected.");
          snprintf (msg, MSG_BUFFER_SIZE, "IP address: %s, Broker: %s Connected", WiFi.localIP().toString().c_str(), eep.mqtt_broker);
          pub("sysTopic", msg);
          sub("logger/set/#");
        //  Log.noticeln("Subscription OK to the");
        } 
      else{
          mqttflag = false;        
          Log.noticeln("MQTT Broker Connected Failed");
      }
    }
    else Log.noticeln("WiFi Connecting Failed");
  delay(200);
}

void eeprom_read_setup(){
    char c[6];
    uint8_t host = EEPROM.read(0x00);
    if((host & 0x20) == 0) pecflag = false;
    else pecflag = true;
    if((host & 0x40) == 0) pmbusflag = false;
    else pmbusflag = true;
    if((host & 0x80) == 0){
      strncpy(eep.ssid, ssid, 16);
      strncpy(eep.password, password, 16);
      strncpy(eep.mqtt_broker, mqtt_server, 31);
    }
    else EEPROM.get(0, eep);
    sprintf(c, "0x%02X:", host);
    Log.noticeln("EE read Host:%s", c);
}

void subMQTT(const char* topic) {
   if(client.subscribe(topic)){
     Log.traceln(F("Subscription OK to the subjects %s"), topic);;
  } else {
    Log.traceln(F("Subscription Failed, rc=%d"), client.state());
  }
}

void sub(const char* topicori) {
  String topic = String(mqtt_topic) + String(topicori);
  subMQTT(topic);
}

void subMQTT(String topic) {
  subMQTT(topic.c_str());
}

void pubMQTT(const char* topic, const char* payload, bool retainFlag) {
  if (client.connected()) {
    Log.traceln(F("[MQTT_publish] topic: %s msg: %s "), topic, payload);
    client.publish(topic, payload, retainFlag);
  } else {
    Log.traceln(F("Client not connected, aborting thes publication"));
  }
}

void pubMQTT(const char* topic, const char* payload) {
  pubMQTT(topic, payload, false);
}

void pubMQTT(String topic, const char* payload) {
  pubMQTT(topic.c_str(), payload);
}

void pubMQTT(String topic, String payload) {
  pubMQTT(topic.c_str(), payload.c_str());
}

void pub(const char* topicori, const char* payload) {
  String topic = String(mqtt_topic) + String(topicori);
  pubMQTT(topic, payload);
}

void pub(const char* topicori, JsonObject& data) {
  String dataAsString = "";
  serializeJson(data, dataAsString);
  String topic = String(mqtt_topic) + String(topicori);
  pubMQTT(topic, dataAsString.c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
    // String inPayload = "";
    // String currtopic = String(mqtt_topic) + "scpi/set/curr";
   //    byte* p = (byte*)malloc(length + 1);
   //    memcpy(p, payload, length);
   //    p[length] = '\0';
    Log.noticeln(F("Message arrived [ %s ]" ), topic);
   if ((char)payload[0] == '[') {
      for(int i = 1; i < length; i++){   
          smbus_data[i-1] = tohex(payload[3*i-2])*16 + tohex(payload[3*i-1]);
          if (i >= 127) {
              Log.noticeln(F("Smbus Invalid format"));
              pub("pmbus/info", "Smbus Invalid format");
              commandflag = false;
              delay(10);
              break; 
            }
          if (payload[3*i] == ']'){
              commandflag = true;
              buttonflag = false;
              break;
          }                       
          if(payload[3*i] != ' ' && 3*i >= (length-1)) {
             Log.noticeln(F("Space Fail, or No ] Fuffix, Smbus Invalid Format"));
              pub("pmbus/info", "Space Fail, or No ] Suffix, Smbus Invalid Format");
              commandflag = false;
              delay(100);
              break;
          }
      }        
    }
  // for (int i = 0; i < length; i++) {
  //       Log.notice("%c", (char)payload[i]);
  //   }
  Log.noticeln("");
}

void mqttLoop(){
  if(wifistatus){
    if (!client.connected()) {
            reconnect();
            sub("logger/set/#");       
        }
    if (mqttflag){    
          client.loop();      
        }
    }
  else {
       setWifiMqtt();
    }
}

void reconnect() {
    // Loop until we're reconnected  
  int k = 0;//client.connect(clientID, mqtt_user, mqtt_password);
  while (!client.connected()) {
    Serial.println(F("Attempting MQTT connection..."));
    //  Log.noticeln(F("Attempting MQTT connection...")); // Attempt to connect   
       //     String clientId = "ESP8266Client-";
       //     clientId += String(random(0xffff), HEX);
      String client_id;
      client_id = clientID + String(WiFi.macAddress());
      client.setServer(eep.mqtt_broker, mqtt_port);
    if (client.connect(client_id.c_str(), mqtt_user, mqtt_password)) {
      Serial.println(F("connected to broker"));
      pub("sysTopic", "Broker reconnected");  // Once connected, publish an announcement...
      mqttflag = true;   
    }
    else {
      // Log.notice("Failed, rc= %d", client.state());
      Serial.printf("Failed, rc= %d\n", client.state());
      Serial.println(F(" try again in 2 seconds"));
      k++;
      delay(2000);   // Wait 2 seconds before retrying
        if( k >= 5){
                mqttflag = false;
                wifistatus = false;
                Serial.println(F("WiFi connect Failed!!"));
                Log.begin(LOG_LEVEL, &Serial, false);
                delay(100);
                break;
            }                   
        }
    }
}

uint8_t tohex(uint8_t val){
  uint8_t hex;
  if ( val - '0' >= 0 && val - '0' <=9){
     hex = val - '0';
  }
  else if( val == 'a' || val == 'A') hex = 10;
  else if( val == 'b' || val == 'B') hex = 11;
  else if( val == 'c' || val == 'C') hex = 12;
  else if( val == 'd' || val == 'D') hex = 13;
  else if( val == 'e' || val == 'E') hex = 14;
  else if( val == 'f' || val == 'F') hex = 15;
  else {
    Log.noticeln(F("Invalid Hex Data" ));
    dataflag = false;
    delay(10);
    return 0;
  }
  dataflag = true;
  return hex;
}

void i2cdetects(uint8_t first, uint8_t last) {
  uint8_t i, address, rerror;
  int q = 0;
  char c[6];
  char addr[35];
  Log.notice("   ");            // table header
  for (i = 0; i < 16; i++) {
    sprintf(c, "%3x",  i);
    Log.notice("%s", c);
  }
  for (address = 0; address <= 127; address++) {             // addresses 0x00 through 0x77
    if (address % 16 == 0) {                            // table body
          sprintf(c, "0x%02x:", address & 0xF0);
          Log.notice(CR "%s", c);
      }
    if (address >= first && address <= last) {
        Wire.beginTransmission(address);
        rerror = Wire.endTransmission();
        delay(10);
        if (rerror == 0) {                           // device found
          sprintf(c, " %02x", address);
          Log.notice("%s", c);
          addr[3*q] = hex_table[address >> 4];
          addr[3*q + 1] = hex_table[address & 0x0f];
          addr[3*q + 2] = ' ';
          q++;
      } else if (rerror == 4) {    // other error      
        Log.notice(" XX");
      } else {                   // error = 2: received NACK on transmit of address              
        Log.notice(" --");    // error = 3: received NACK on transmit of data
      }
    } else {                 // address not scanned      
      Log.notice("   ");
    }
    if(q > 10) break;
  }
  addr[3*q] = '\0';
  Log.noticeln("" CR);
  snprintf (msg, MSG_BUFFER_SIZE, "Scan addr at:0x%s", addr);
  pub("pmbus/info", msg); 
}

void pmbusflagset(uint8_t val){
  if(val == 0) {
    pmbusflag = false;
    Log.noticeln(F("pmbusflag Disable"));
    pub("pmbus", "0");
  }
  else {
    pmbusflag = true;
    Log.noticeln(F("pmbusflag Enable"));
    pub("pmbus", "1");
  }
  delay(100);
}

void setpecstatus(uint8_t val){
    if(val == 0) {
      Log.noticeln(F("PEC Disable"));
      pecflag = false;
      pub("pec", "0");
    }
    else {
      Log.noticeln(F("PEC Enable"));
      pecflag = true;
      pub("pec", "1");
    }
    delay(100);
}

void ledflash(){
  digitalWrite(kLedPin, !digitalRead(kLedPin));
}

void set_custom(uint8_t hostval){
  EEPROM.write(0x00, hostval);
  EEPROM.commit();
  Log.noticeln("Host Write:%x", hostval);
  delay(50);
}

void set_wifi2eeprom(){
      Log.noticeln("");
      EEPROM.get(0, eep);
      Log.noticeln(F("Old host set: %x"), eep.host);
      Serial.println("Old values are: "+String(eep.ssid)+", "+String(eep.password)+", "+String(eep.mqtt_broker));
      Log.noticeln(F("Input host set: (0x000 is default setting, 0x80 host from EE, 0x40 PMbus Enable, 0x20 PEC Eanble)"));
      eep.host = read_int();     
      Log.noticeln(F("New host set: %x"), eep.host);
      Log.noticeln(F("Input WiFi ssid:"));
      strncpy(eep.ssid, read_string(), 16);
      Log.noticeln(F("New WiFi ssid: %s"), eep.ssid);
      Log.noticeln(F("Input WiFi password:"));
      strncpy(eep.password, read_string(), 16);
      Log.noticeln(F("New WiFi password: %s"), eep.password);
      Log.noticeln(F("Input MQTT broker:"));
      strncpy(eep.mqtt_broker, read_string(), 31);
      Log.noticeln(F("New MQTT broker: %s"), eep.mqtt_broker);
      EEPROM.put(0,eep);
      Log.noticeln(F("Do you want to save the data to EEPROM. Yes(Y), No(N):"));
      char val;
      val = read_char(); 
      if ((char)val == 'y'  || (char)val == 'Y') {
      EEPROM.commit();
      Log.noticeln(F("EEprom Write Save"));
      delay(20);
      }
      else Log.noticeln(F("EEprom Without Save Exit"));
      delay(50);
}

void set_broker(){
     Log.noticeln("");
     EEPROM.get(0, eep);
     Serial.println("EEPROM Broker is: " +String(eep.mqtt_broker));
     Log.noticeln(F("Input New MQTT broker:"));
     strncpy(eep.mqtt_broker, read_string(), 31);
     Log.noticeln(F("New MQTT broker: %s"), eep.mqtt_broker);
     Log.noticeln(F("Commit the new data to EEPROM. Yes(Y), No(N):"));
      char val;
      val = read_char(); 
      if ((char)val == 'y'  || (char)val == 'Y') {
      EEPROM.commit();
      Log.noticeln(F("Data Commit"));
      delay(20);
      }
      else Log.noticeln(F("Data Without Save"));
}

void set_host(){
  Log.noticeln(F("host read from eeprom: %d"), EEPROM.read(0));     
  Log.noticeln(F("Input host: (0x00 for default, 0x80 read setup from eeprom, 0x40 Enable Pmbus read, 0x20 PEC Enable.)"));
  eep.host = read_int();     
  Log.noticeln(F("New host set: %d"), eep.host);
  EEPROM.write(0, eep.host);
  Log.noticeln(F("Do you want to save the New host data to EEPROM. Yes(Y), No(N):"));
  char val;
  val = read_char(); 
  if ((char)val == 'y'  || (char)val == 'Y') {
      EEPROM.commit();
      delay(30);
  }
  // delay(50);
}

void esprestar(){
    Log.noticeln(F("delay 3S Reset "));
    for(int i = 0; i < 6; i++){
      delay(500);
      Log.notice(" .");
    }
    ESP.restart();
}

void printhelp(){
      Log.noticeln(F("************* Temperature Record **************"));
      Log.notice(F(" * > ESP8266 Device ID topic: %s " CR), DEVICE_ID_Topic);
      Log.notice(F(" 0 > pub [AA 01 58] Set the PMbus Device Address" CR));
      Log.notice(F(" 1 > pub [AA 01 XX XX] Set pmbus poll time /ms" CR));
      Log.notice(F(" 2 > pub [AA 02 00] Disable PMbus " CR));
      Log.notice(F(" 3 > pub [AA 03 01/00] PEC Enable/Disable." CR));
      Log.notice(F(" 4 > pub [AA A0 00/80] WiFI MQTT broker from EEPROM" CR));
      Log.notice(F(" 5 > pub [AA BB] RESET DEVICE" CR));
      Log.notice("" CR);     
      delay(100);  
}

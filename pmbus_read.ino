uint8_t    runningpec;     //!< Temporary pec calc value
void pecClear(void)
 {
  runningpec = 0;
 }

void pecAdd(uint8_t byte_value)
  {
  uint8_t i;
  runningpec = runningpec ^ byte_value;
  for (i=0; i<8; i++)
  {
    if ((runningpec & 0x80) > 0x00)
    {
      runningpec = (runningpec << 1) ^ 0x07; //0x07 poly used in the calc
    }
    else
    {
      runningpec = runningpec << 1;
    }
  }
 }

uint8_t pecGet(void)
 {
    return runningpec;
 }

float L11_to_float(uint16_t input_val)
 {
  int8_t exponent = input_val >> 11;             // extract exponent as MS 5 bits
  int16_t mantissa = input_val & 0x7ff;         // extract mantissa as LS 11 bits
  if( exponent > 0x0F ) exponent |= 0xE0;      // sign extend exponent from 5 to 8 bits
  if( mantissa > 0x03FF ) mantissa |= 0xF800; // sign extend mantissa from 11 to 16 bits
  return mantissa * pow(2.0,exponent);       // compute value as mantissa * 2^(exponent)
 }

float L16_to_float_mode(uint8_t expvmode, uint16_t input_val)
 {
  int8_t exponent = expvmode;             // Assume Linear 16, pull out 5 bits of exponent, and use signed value.
  float mantissa = (float)input_val;      // int8_t exponent = (int8_t) vout_mode & 0x1F; Convert mantissa to a float so we can do math.
  if( exponent > 0x0F ) exponent |= 0xE0;   // sign extend exponent
  return mantissa * pow(2.0,exponent);
 }

uint8_t pmbus_waitForAck(uint8_t address, uint8_t command) //! Read with the address and command in loop until ack, then issue stop 
 {                                                         
  uint8_t data;   // A real application should timeout at 4.1 seconds.
 // uint16_t timeout = 8192;
  uint16_t timeout = 32;   
  while (timeout-- > 0)
  {  
    if (0 == i2c_WriteRead(address, 1, &command, 1, &data))
    return 1;    
    delay(1);
  }
  return 0;    
 }

int8_t i2c_WriteRead(uint8_t address, uint8_t clength, uint8_t *commands, uint8_t length, uint8_t *values)
 {
  uint8_t i = clength;   // int8_t ret = 0;
  bool Protocol;
  uint8_t pec;  
  if (length == 0) {
      Protocol = true;
      if (pecflag)
        {
          uint16_t pos = 0;
          pecClear();
          pecAdd(address << 1);
          while (pos < clength)
          pecAdd(commands[pos++]);
          pec = pecGet();
        }

    Wire.beginTransmission(address);
    do
      {
        i--;
      }
    while (Wire.write(commands[clength - 1 - i]) == 1 && i > 0);
    if(pecflag) Wire.write(pec);

     if (Wire.endTransmission(Protocol)) // endTransmission(false) is a repeated start; endTransmission returns zero on success
      {  
        Wire.endTransmission();
        return(1);
      }
    return(0);
  }

  else {  
    Protocol = false;
    Wire.beginTransmission(address);
    do
      {
        i--;
      }
    while (Wire.write(commands[clength - 1 - i]) == 1 && i > 0);
      if (Wire.endTransmission(Protocol)) // endTransmission(false) is a repeated start
        {        
          Wire.endTransmission();
          return(1);
        }    
      uint8_t readBack = 0;
      Protocol = true;
      readBack = Wire.requestFrom((uint8_t)address, (uint8_t)length, (uint8_t)true);
      if(readBack == length)
        {
          while (Wire.available())
        {
          values[i] = Wire.read();
          if (i == (length-1)) break;        
          i++;
        }
        return (0);
      }
    else
      {
        return (1);    
      }
   }
 }

float pmbus_read_float(uint8_t address, uint8_t commands, bool v_mode = false)
 {
    uint8_t data[2];
    uint8_t vout_mode = 0x20;
    uint8_t vout_data;
    if(v_mode)
    {
      if(i2c_WriteRead(address, 1, &vout_mode, 1, &vout_data))
      Serial.println("read vout fail");
      i2c_WriteRead(address, 1, &commands, 2, data);
      return L16_to_float_mode(vout_data, data[1] << 8 | data[0]);
    }
    if(i2c_WriteRead(address, 1, &commands, 2, data))
        Serial.println("read fail");
    return L11_to_float(data[1] << 8 | data[0]);  
 }

uint16_t pmbus_read_int(uint8_t address, uint8_t commands)
 {
    uint8_t data[2];
    if(i2c_WriteRead(address, 1, &commands, 2, data))
    Serial.println("read fail");
    return data[1] << 8 | data[0];
 }

uint8_t pmbus_read_byte(uint8_t address, uint8_t commands)
 {
    uint8_t data;
    if(i2c_WriteRead(address, 1, &commands, 1, &data))
    Serial.println("read fail");
    return data;
 }

void pmbus_read_block(uint8_t address, uint8_t commands, uint8_t *block, uint16_t block_size)
 {
    // uint8_t data;
    if(i2c_WriteRead(address, 1, &commands, block_size, block))
    Serial.println("read fail");
 }

bool pmbusread(){   
      bool ret = true;
      if(!pmbusflag) return ret = false;  
      if(pmbus_waitForAck(ps_i2c_address, 0x00) == 0) {  //0x00 PAGE read         
          if(wifistatus && mqttflag){
            if(count%6 == 0){
              ++value;     
              snprintf (msg, MSG_BUFFER_SIZE, "PMBUS Polling Fail Loop#%ld", value);
              pub("pmbus/status", msg);
            }
         }
          Log.noticeln(F("PMBUS Polling Fail loop: %l, Type 'h' To Help"), count);
          delay(10);      
          return ret = false;
       }       
     ld.statusWord = pmbus_read_int(ps_i2c_address, 0x79);
     ld.temp8E = pmbus_read_float(ps_i2c_address, 0x8E);        //temp sensor 0x8E  
     ld.temp8F = pmbus_read_float(ps_i2c_address, 0x8F);        //temp sensor 0x8F  
     return ret;     
}

void publishLog(){
  DynamicJsonDocument doc(1024);
  JsonObject logger = doc.createNestedObject("logger");  
  if (count%6 == 0) {
          ++value;     
          snprintf (msg, MSG_BUFFER_SIZE, "LOGGER: Refresh#%ld", value);
          pub("logger/status", msg);
          Log.noticeln("LOGGER_PUBLISH_REFRESH:%d", value);
        }
  logger["counter"] =  count;
  logger["Time"] =  ld.unixtime;
  logger["TempKa"] =  ld.tempKa;
  logger["TempKb"] =  ld.tempKb;
  logger["TempLm73"] =  ld.tempLm73;
  if(pmbusflag){
         logger["Temp8D"] = ld.temp8D;
         logger["Temp8E"] = ld.temp8E;
         logger["Temp8F"] = ld.temp8F;
         logger["Fan"] = ld.fanSpeed;
         logger["PMstatus"] = ld.statusWord;
  }
  // String jsonString;
  // serializeJson(doc, jsonString);
  pub("logger/jsoninfo", logger); 
}

void printLoggerData(){
    Log.noticeln(F("========== LOGGER DATA =========="));
    Log.noticeln(F("TP_Ka:%F, TP_Kb:%F, TP_lm73:%F"), ld.tempKa, ld.tempKb, ld.tempLm73);
    if(pmbusflag){
      Log.noticeln(F("T_8D:%F, T_8E:%F, T_8F:%F, FAN:%F"), ld.temp8D, ld.temp8E, ld.temp8F, ld.fanSpeed);
      Log.noticeln(F("STATUS WORD: %X,  %B"), ld.statusWord, ld.statusWord);
    }
    Log.noticeln(F(" "));
}

void lm73_init()
  {
    Wire.beginTransmission(LM73_ADDR);
    Wire.write(0x04);
    Wire.write(0x40);
    Wire.endTransmission();
    delay(10);
  }

float get_lm73_temp(){
  unsigned data[2];
  Wire.beginTransmission(LM73_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(1);
  Wire.requestFrom(LM73_ADDR,2);
  if (Wire.available() == 2)
  {
    data[0]=Wire.read();
    data[1]=Wire.read();
  }
  //int tempscale = data[0];
  int temp = ((data[0] << 8) | data[1]) >> 3;
  if (temp >= 0x1000)
  {
    temp -= 0x2000;
  }
 float cTemp = temp * 0.0625;
 return cTemp;
}


uint8_t read_data()
{
  uint8_t index = 0; //index to hold current location in ui_buffer
  int c; // single character used to store incoming keystrokes
  while (index < UI_BUFFER_SIZE-1)
  {
    ESP.wdtFeed();      // wdtFeed or delay can solve the ESP8266 SW wdt reset while is waiting for serial data
//    delay(1);
    c = Serial.read(); //read one character
    if (((char) c == '\r') || ((char) c == '\n')) break; // if carriage return or linefeed, stop and return data
    if ( ((char) c == '\x7F') || ((char) c == '\x08') )   // remove previous character (decrement index) if Backspace/Delete key pressed      index--;
    {
      if (index > 0) index--;
    }
    else if (c >= 0)
    {
      ui_buffer[index++]=(char) c; // put character into ui_buffer
    }
  }
  ui_buffer[index]='\0';  // terminate string with NULL

  if ((char) c == '\r')    // if the last character was a carriage return, also clear linefeed if it is next character
  {
    delay(10);  // allow 10ms for linefeed to appear on serial pins
    if (Serial.peek() == '\n') Serial.read(); // if linefeed appears, read it and throw it away
  }

  return index; // return number of characters, not including null terminator
}

int32_t read_int()
{
  int32_t data;
  read_data();
  if (ui_buffer[0] == 'm')
    return('m');
  if ((ui_buffer[0] == 'B') || (ui_buffer[0] == 'b'))
  {
    data = strtol(ui_buffer+1, NULL, 2);
  }
  else
    data = strtol(ui_buffer, NULL, 0);
  return(data);
}

int8_t read_char()
{
  read_data();
//  delay(1);
  return(ui_buffer[0]);
}

// Read a string from the serial interface.  Returns a pointer to the ui_buffer.
char *read_string()
{
  read_data();
  return(ui_buffer);
}
/* Smart Watering project
 * JRY 2016*/
#define REVISION "1.0.0"
#define AUTHOR "JRY"
 /* Board: Particle Core
 * Pins:
 *    D0  -> SDA
 *    D1  -> SCL
 *    D3  -> Water counter interrupt (has to be 5V tolerant!)
 *    D2  -> Water pump control
 */

//#define DEBUG_MODE
#ifdef DEBUG
#undef DEBUG
#endif

#ifdef DEBUG_MODE
  #define DEBUG(x) Serial.println(x)
#else
  #define DEBUG(x) 
#endif

int MOISTURE_SENSOR_I2C_ADDR = 0x20;
#define CAPACITIVE_MEAS_AVERAGING 10
#define CAPACITIVE_THRESHOLD 400
#define WATERING_BASE_QUANTITY 30
#define WATERING_TIMEOUT 5000 //milliseconds
#define PUMP D3
#define MAINLOOP_BASE_DELAY_MS 100
#define MOISTURE_CHECK_DELAY_MS 300000  //5 minutes
#define SOILMOISTURESENSOR_SET_ADDRESS 0x01 //	(w) 	1 byte
#define SOILMOISTURESENSOR_GET_ADDRESS 0x02 // (r) 	1 byte
#define SOILMOISTURESENSOR_RESET 0x06 //	(w) 	n/a

#define CLI_CMD1 "info"     //get metadata about the system
#define CLI_CMD2 "moisture" //require the moisture value
#define CLI_CMD3 "temp"     //require the temperature value
#define CLI_CMD4 "start"    //start a watering cycle
#define CLI_CMD5 "lumi"     //require the luminosity value
#define CLI_CMD6 "LastFeed" //require the minutes elapsed since last feed
#define CLI_CMD7 "SetAddress"
#define CLI_CMD8 "ChangeSensor"
#define CLI_CMD9 "ScanForSensor"
#define CLI_CMD10 "GetValuesFromSensors"

#define PUBLISH_EVENT_NAME "MyGarden1"
#define PUBLISH_EVENT_NAME_ERROR "MyError"

#define RXBUFLEN 200
char RXStr[RXBUFLEN];
char RXPtr = 0;

volatile bool IsWatering = false;
volatile int LastMoistureMeasured = 0;
volatile unsigned long int MoistureDelayAccu = 0;
char tmp[200];

//cloud sync functions &variables
int CloudRequest(String req);
int Capa = 0;
int Temp = 0;
int Moisture = 0;
int Lumi = 0;
int LastFeed = -1;

unsigned int getTemp(int addr){
  Wire.beginTransmission(addr); // give address
  Wire.write(0x05);            // sends instruction byte
  Wire.endTransmission();     // stop transmitting
  Wire.requestFrom(addr, 2);
  while (!Wire.available())
    delay(2);
  unsigned char a = Wire.read(); // receive a byte as character
  while (!Wire.available())
    delay(2);
  unsigned char b = Wire.read(); // receive a byte as character
  Temp = (a * 256 + b) / 10;
  return Temp;
}

unsigned int getLumi(int addr){
  Wire.beginTransmission(addr); // give address
  Wire.write(0x03);            // sends instruction byte
  Wire.endTransmission();     // stop transmitting
  delay(200);
  Wire.beginTransmission(addr); // give address
  Wire.write(0x04);            // retrieve the last Luminosity value 
  Wire.endTransmission();     // stop transmitting
  Wire.requestFrom(addr, 2);
  while (!Wire.available())
    delay(2);
  unsigned char a = Wire.read(); // receive a byte as character
  while (!Wire.available())
    delay(2);
  unsigned char b = Wire.read(); // receive a byte as character
  Lumi = ((a<<8) + b);
  return Lumi;
}

int getCapa(int addr) {
  Wire.beginTransmission(addr); // give address
  Wire.write(0x00);            // sends instruction byte
  Wire.endTransmission();     // stop transmitting
  Wire.requestFrom(addr, 2);
  while (!Wire.available())
    delay(2);
  char a = Wire.read(); // receive a byte as character
  while (!Wire.available())
    delay(2);
  char b = Wire.read(); // receive a byte as character
  Capa = ((a<<8) + b);
  return Capa;
}

int MoistureValue(int addr){
  int acc = 0;
  getCapa(addr);//dummy Capa read in order to finally get a current value
  for (int i = 0; i < CAPACITIVE_MEAS_AVERAGING; i++)
  {
    acc += getCapa(addr);
  }
  acc = acc / CAPACITIVE_MEAS_AVERAGING;
  LastMoistureMeasured = acc;
  Moisture = acc;
  return Moisture;
}

bool isHungry(int addr){
  if (MoistureValue(addr) < CAPACITIVE_THRESHOLD)
    return true;
  return false;
}

void Feed(void)
{
  int timeout = 0;
  digitalWrite(PUMP,0);//turn pump ON
  delay(20);//mandatory in order to avoid the "turn pump ONOFF" voltage spike to corrupt serial data
  sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"pump\",\"dataValue\":\"ON\"}\r\n"/*,millis()*/);
  Serial.print(tmp);
  while(timeout<WATERING_TIMEOUT)
  {
    delay(200);
    timeout+=200;
  }
  digitalWrite(PUMP,1);//turn pump OFF 
  
  delay(20);//mandatory in order to avoid the "turn pump ONOFF" voltage spike to corrupt serial data
  sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"pump\",\"dataValue\":\"OFF\"}\r\n"/*,millis()*/);
  Serial.print(tmp);
  if(timeout>=WATERING_TIMEOUT)
  {
    delay(20);//mandatory in order to avoid the "turn pump ONOFF" voltage spike to corrupt serial data
    Serial.print("{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"systemError\",\"dataValue\":\"Pump problem\"}\r\n");
  }
  else
  {
      MoistureDelayAccu=0;
      LastFeed=millis();
  }
}

int setAddress(int addr) {
  Wire.beginTransmission(MOISTURE_SENSOR_I2C_ADDR); // give address
  Wire.write(SOILMOISTURESENSOR_SET_ADDRESS);            // sends instruction byte to set address
  Wire.write(addr);            // sends instruction byte to what address
  Wire.endTransmission();     // stop transmitting
  
  ResetSensor(); // Reset the sensor
  delay(1000); // Delay a second
  
  MOISTURE_SENSOR_I2C_ADDR = addr;
  
  return 1;
}

int ResetSensor(void) {
  Wire.beginTransmission(MOISTURE_SENSOR_I2C_ADDR); // give address
  Wire.write(SOILMOISTURESENSOR_RESET);            // sends instruction byte to set address
  Wire.endTransmission();     // stop transmitting
  return 1;
}

int ChangeSensor(int addr) {
    MOISTURE_SENSOR_I2C_ADDR = addr;
    ResetSensor(); // Reset the sensor
    delay(1000); // Delay a second
    return 1;
}

int ScanForSensor(void) {
    byte error, address;
    int nDevices;
    
    Serial.println("Scanning...");
    
    nDevices = 0;
    
    for(address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16)
            {
                Serial.print("0");
            }
            Serial.print(address,HEX);
            Serial.println("  !");
            
            nDevices++;
        }
        else if (error==4) 
        {
            Serial.print("Unknow error at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
        } else {
            //Serial.println("No sensor at address: ");
            //Serial.println(address,HEX);
        }
    }
    
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");
    
    delay(5000);           // wait 5 seconds for next scan
    
    return 1;
}

int GetValuesFromSensors(void) {
    byte error, address;
    int nDevices;
    
    Serial.println("Scanning...");
    
    nDevices = 0;
    
    for(address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16)
            {
                Serial.print("0");
            }
            Serial.print(address,HEX);
            Serial.println("  !");
            
            bool SensorIsHungry = isHungry(address);
            
            int SensorMoisture = MoistureValue(address);
            
            sprintf(tmp ,"{ \"my-sensor\": \"%i\", \"my-key\": \"%s\", \"my-value\": \"%i\" }\r\n", address, CLI_CMD2, SensorMoisture);
            
            Serial.print(tmp);
            Particle.publish(PUBLISH_EVENT_NAME, tmp, 60, PRIVATE);
            
            // Publish only publish 1 event / sec so delay it one sec for next
            delay(1000);
            
            unsigned int SensorTemp = getTemp(address);
            
            sprintf(tmp, "{ \"my-sensor\": \"%i\", \"my-key\": \"%s\", \"my-value\": \"%i\" }\r\n", address, CLI_CMD3, SensorTemp);
            
            Serial.print(tmp);
            Particle.publish(PUBLISH_EVENT_NAME, tmp, 60, PRIVATE);
            
            // Publish only publish 1 event / sec so delay it one sec for next
            delay(1000);
            
            unsigned int SensorLumi = getLumi(address);
            
            sprintf(tmp, "{ \"my-sensor\": \"%i\", \"my-key\": \"%s\", \"my-value\": \"%i\" }\r\n", address, CLI_CMD5, SensorLumi);
            
            Serial.print(tmp);
            Particle.publish(PUBLISH_EVENT_NAME, tmp, 60, PRIVATE);
            
            if (SensorIsHungry) {
                // Publish only publish 1 event / sec so delay it one sec for next
                delay(1000);
                sprintf(tmp,"{ \"my-sensor\": \"%i\", \"my-key\": \"IsHungry\", \"my-value\": \"true\" }\r\n", address);
                
                Serial.print(tmp);
                Particle.publish(PUBLISH_EVENT_NAME, tmp, 60, PRIVATE);
            }
            
            nDevices++;
        }
        else if (error==4) 
        {
            // Publish only publish 1 event / sec so delay it one sec for next
            delay(1000);
            
            Serial.print("Unknow error at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
            
            sprintf(tmp,"{ \"my-sensor\": \"%i\", \"my-key\": \"ERROR\", \"my-value\": \"%i\" }\r\n", address, error);
            
            Particle.publish(PUBLISH_EVENT_NAME_ERROR, tmp, 60, PRIVATE);
        } else {
            //Serial.println("No sensor at address: ");
            //Serial.println(address,HEX);
            /*
                0: success
                1: busy timeout upon entering endTransmission()
                2: START bit generation timeout
                3: end of address transmission timeout
                4: data byte transfer timeout
                5: data byte transfer succeeded, busy timeout immediately after
            */
            if (address < 5) {
                // Publish only publish 1 event / sec so delay it one sec for next
                delay(1000);
                
                sprintf(tmp,"{ \"my-sensor\": \"%i\", \"my-key\": \"ERROR\", \"my-value\": \"%i\" }\r\n", address, error);
                
                Particle.publish(PUBLISH_EVENT_NAME_ERROR, tmp, 60, PRIVATE);
            }
        }
        
        // Publish only publish 1 event / sec so delay it one sec for next
        delay(1000);
    }
    
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");

    return 1;
}

int CloudRequest(String req)
{
    if(req==CLI_CMD4)
    {
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"moisture\",\"dataValue\":\"%i\"}\r\n",MoistureValue(MOISTURE_SENSOR_I2C_ADDR));
      Serial.print(tmp);
      Feed();
      MoistureDelayAccu=0;
      return 0;
    }
    else if(req==CLI_CMD2)
    {
        return MoistureValue(MOISTURE_SENSOR_I2C_ADDR);
    }
    else if(req==CLI_CMD3)
    {
        return getTemp(MOISTURE_SENSOR_I2C_ADDR);
    }
    else if(req==CLI_CMD5)
    {
        return getLumi(MOISTURE_SENSOR_I2C_ADDR);
    }
    else if(req==CLI_CMD6)
    {
        if(LastFeed>-1)
            return (millis()-LastFeed)/60000;
        else
            return -1;
    }
    else
    {
        Serial.print("unknown Cloud command: ");
        return -1;
    }
}

void CLI(char * cmd){
  if(cmd[0]=='s' && cmd[1]=='w' && cmd[2]==' ')
  {
    if(strstr(cmd,CLI_CMD1)){
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"info\",\"dataValue\":{\"Project\":{\"revision\":\"%s\",\"author\":\"%s\"},\"system\":{\"time\":\"%lu\"}}}\r\n",REVISION,AUTHOR,millis());
      Serial.print(tmp);
    }
    else if(strstr(cmd,CLI_CMD2)){
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"moisture\",\"dataValue\":\"%i\"}\r\n",MoistureValue(MOISTURE_SENSOR_I2C_ADDR));
      Serial.print(tmp);
    }   
    else if(strstr(cmd,CLI_CMD3)){
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"temperature\",\"dataValue\":\"%i\"}\r\n",getTemp(MOISTURE_SENSOR_I2C_ADDR));
      Serial.print(tmp);
    }
    else if(strstr(cmd,CLI_CMD4)){
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"moisture\",\"dataValue\":\"%i\"}\r\n",MoistureValue(MOISTURE_SENSOR_I2C_ADDR));
      Serial.print(tmp);
      Feed();
      MoistureDelayAccu=0;
    }
    else if(strstr(cmd,CLI_CMD5)){
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"luminosity\",\"dataValue\":\"%d\"}\r\n",getLumi(MOISTURE_SENSOR_I2C_ADDR));
      Serial.print(tmp);
    }
    else if(strstr(cmd,CLI_CMD6)){
        sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"lastFeed\",\"dataValue\":\"%d\"}\r\n",(LastFeed>-1)?(millis()-LastFeed)/60000:-1);
        Serial.print(tmp);
    }
    else if(strstr(cmd,CLI_CMD7)){
        int c = (int)cmd[14] - 48;
        
        setAddress(c);
        
        sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"AddressSetTo\",\"dataValue\":\"%i\"}\r\n", c);
        Serial.print(tmp);
    }
    else if(strstr(cmd,CLI_CMD8)){
      int c = (int)cmd[16] - 48;
      
      ChangeSensor(c);
      
      sprintf(tmp,"{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"ChangeSensorTo\",\"dataValue\":\"%i\"}\r\n", c);
      Serial.print(tmp);
    }
    else if(strstr(cmd,CLI_CMD9)){
      ScanForSensor();
      
      Serial.print("{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"ScanForSensor\",\"dataValue\":\"Done\"}\r\n");
    }
    else if(strstr(cmd,CLI_CMD10)){
      GetValuesFromSensors();
      
      Serial.print("{\"device\":\"sw\",\"type\":\"data\",\"dataPoint\":\"GetValuesFromSensors\",\"dataValue\":\"Done\"}\r\n");
    }
    else
      Serial.println("unknown parameter");
  }
  else
    Serial.println("unknown command");
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, 1);

  //register cloud functions & variables
  Particle.function("CloudRequest", CloudRequest);
  //Particle.variable("Temp", Temp);
  //Particle.variable("Lumi", Lumi);
  //Particle.variable("Moisture", Moisture);
}

void loop() {  
  int CapaValue;

  if (Serial.available() > 0) {
    RXStr[RXPtr++] = Serial.read();
    Serial.print(RXStr[RXPtr-1]);//echo each character
    if (RXPtr == RXBUFLEN)
      RXPtr=0;
    if (strstr(RXStr,"\n") || strstr(RXStr,"\r"))
    {
        Serial.print("\r\n");
        RXStr[RXPtr] = 0;//add a NULL 
        CLI(RXStr);
        RXStr[RXPtr-1] = 0;//Invalidate the termination symbol
        RXPtr = 0;//Reset the Buffer pointer
    }
  }
  else
  {
    delay(MAINLOOP_BASE_DELAY_MS);
    MoistureDelayAccu += MAINLOOP_BASE_DELAY_MS;
    
    if (MoistureDelayAccu >= MOISTURE_CHECK_DELAY_MS)
    {
        MoistureDelayAccu = 0;
        
        GetValuesFromSensors();
    }    
  }
}
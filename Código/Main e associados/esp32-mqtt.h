/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
// This file contains static methods for API requests using Wifi / MQTT

#include <Wire.h>                     //Bib. para fazer a conexão I2C
#include "DHT.h"                      //Bib. Sensor de Temperatura e Humidade
#include "MutichannelGasSensor.h"     //Bib. Sensor de Gás
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>

#include <CloudIoTCore.h>
#include "ciotc_config.h"             //Ficheiro com as configurações para a ligação à cloud
#ifndef __ESP32_MQTT_H__
#define __ESP32_MQTT_H__
#define DHTPIN 14                     //Pin ao qual o sensor de temperatura está conectado

DHT dht(DHTPIN);                      //Definir qual o pin e o tipo para fazer a inicialização (dht.begin)  




// Initialize the Genuino WiFi SSL client library / RTC
WiFiClientSecure *netClient;
MQTTClient *mqttClient;

// Clout IoT configuration that you don't need to change
CloudIoTCoreDevice *device;
unsigned long iss = 0;
String jwt;

const int LED_BUILTIN=2;

///////////////////////////////
// Helpers specific to this board
///////////////////////////////

String valores_sensores()
{
  float temp, hum, gasCO, gasNO2;
  int i, soma=0, som=0, flag_som=0, iQA;
  float temp_erro, hum_erro, som_dB;
  
  //Leitura de todos os sensores
  temp=dht.readTemperature();   		//Leitura temperatura
  hum=dht.readHumidity();     			//Leitura humidade
  som=analogRead(A7);					//Leitura som
  gasCO=gas.measure_CO();     			//Leitura CO
  gasNO2=gas.measure_NO2();   			//Leitura NO2

  //Cálculo iQA
  if((gasCO>0 && gasCO<=4.36) || (gasNO2>0 && gasNO2<=0.02))
  {
    iQA=5;    //Muito Bom
  }

  if((gasCO>4.36 && gasCO<=6.10) || (gasNO2>0.02 && gasNO2<=0.052))
  {
    iQA=4;    //Bom
  }

  if((gasCO>6.10 && gasCO<=6.98) || (gasNO2>0.052 && gasNO2<=0.09))
  {
    iQA=3;    //Médio
  }

  if((gasCO>6.98 && gasCO<=8.72) || (gasNO2>0.09 && gasNO2<=0.2))
  {
    iQA=2;    //Fraco
  }

  if(gasCO>8.72 || gasNO2>0.2)
  {
    iQA=1;    //Mau
  }
 
    
  Serial.print("Temperatura: "); Serial.print(temp); Serial.println(" ºC");
  Serial.print("Humidade: "); Serial.print(hum); Serial.println("%");
  

  //Verificação de erro som e print valor
  if (isnan(som) || som<=0)
  {
    Serial.println("Erro leitura sensor som");
	flag_som=1;
  }
  
  //Digital para dB
  if (flag_som==0)
  {
	  if (som>=248 && som<=251)							//1º Bloco
	  {
		  som_dB=((5.0/3.0)*float(som))-(1165.0/3.0);
	  }
	  
	  else if (som>=252 && som<=259)					//2º Bloco
	  {
		  som_dB=((5.0/4.0)*float(som))-283.75;
	  }
	  
	  else if (som>=260 && som<=262)					//3º Bloco
	  {
		  som_dB=((10.0/3.0)*float(som))-(2470.0/3.0);
	  }
	  else if (som>=263 && som<=268)					//4º Bloco
	  {
		  som_dB=((5.0/3.0)*float(som))-(1160.0/3.0);
	  }
	  
	  else if (som>=269 && som<=270)					//5º Bloco
	  {
		  som_dB=5.0*float(som)-1280;
	  }
	  else if (som>=271 && som<=310)					//6º Bloco
	  {
		  som_dB=((1.0/4.0)*float(som))+(5.0/2.0);
	  }
	  
	  else if (som>=311 && som<=433)					//7º Bloco
	  {
		  som_dB=((5.0/123.0)*float(som))-(8290.0/123.0);
	  }
	  else
		  som_dB=((5.0/88.0)*float(som))-(5315.0/88.0);
	Serial.print("Som: "); Serial.print(som_dB); Serial.println("dB");
  }
  


  //Verificação de erro CO/NO2 e print valor
  if (isnan(gasCO) || isnan(gasNO2) || gasCO<=0 || gasNO2<=0)
  {
    Serial.println("Erro leitura sensor gás");
  }
  else
  {
    Serial.print("CO: "); Serial.print(gasCO); Serial.print("ppm <=> "); Serial.print(((gasCO*28)/24.45)*1000); Serial.println("ug/m3");
    Serial.print("NO2: "); Serial.print(gasNO2); Serial.print("ppm <=> "); Serial.print(((gasNO2*46)/24.45)*1000); Serial.println("ug/m3");
  }

  switch (iQA) {
      case 1:
        Serial.println("Índice Qualidade do Ar: Mau");
          break;
      case 2:
      Serial.println("Índice Qualidade do Ar: Fraco");
          break;
    case 3:
      Serial.println("Índice Qualidade do Ar: Médio");
      break;
    case 4:
      Serial.println("Índice Qualidade do Ar: Bom");
      break;
    case 5:
      Serial.println("Índice Qualidade do Ar: Muito Bom");
      break;
  }
  Serial.println("-----------------------------");

  
  return String("t,")+String(temp)+String(";h,")+String(hum)+String(";c,")+String(gasCO)+String(";n,")+String(gasNO2)+String(";s,")+String(som);
}

String getJwt() {   
  iss = time(nullptr);
  Serial.println("Refreshing JWT");
  jwt = device->createJWT(iss);
  return jwt;
}

void setupWifi() {
  Serial.println("Starting wifi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967) {
    delay(10);
  }
}


///////////////////////////////
// MQTT common functions
///////////////////////////////
void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
}


//Início da comunicação MQTT
void startMQTT() {
  mqttClient->begin("mqtt.googleapis.com", 8883, *netClient);
}

void publishTelemetry(String data) {
  mqttClient->publish(device->getEventsTopic(), data);
}


void publishState(String data) {
  mqttClient->publish(device->getStateTopic(), data);
}

void mqttConnect() {
  Serial.print("\nconnecting...");
  while (!mqttClient->connect(device->getClientId().c_str(), "unused", getJwt().c_str(), false)) {
    Serial.println(mqttClient->lastError());
    Serial.println(mqttClient->returnCode());
    delay(1000);
  }
  Serial.println("\nconnected!");
  mqttClient->subscribe(device->getConfigTopic());
  mqttClient->subscribe(device->getCommandsTopic());
  publishState("connected");
}

///////////////////////////////
// Orchestrates various methods from preceeding code.
///////////////////////////////
void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  mqttConnect();
}

//É feita a configuração da nuvem com os dados preenchidos em "ciotc_config.h"
//E iniciada a comunicação
void setupCloudIoT() {
  device = new CloudIoTCoreDevice(
      project_id, location, registry_id, device_id,
      private_key_str);

  setupWifi();
  netClient = new WiFiClientSecure();
  mqttClient = new MQTTClient(512);
  startMQTT();
}
#endif //__ESP32_MQTT_H__

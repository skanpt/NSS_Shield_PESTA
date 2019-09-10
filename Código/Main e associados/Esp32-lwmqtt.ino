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
#include "esp32-mqtt.h"
#define usec_to_min 60000000  									//Microsegundos para minutos
#define sleeptime_min  15        								//Tempo que o esp32 fica em deepsleep mode (em minutos)

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  esp_sleep_enable_timer_wakeup(sleeptime_min*usec_to_min);		//Configurar deepsleep mode
  dht.begin();         											//pinMode e digitalWrite usando os valores definidos por dht(DHTPIN);
  gas.begin(0x04);      										//Endereço I2C padrão da slave (0x04)
  gas.powerOn();  
  setupCloudIoT();
}

                   
void loop() {
  mqttClient->loop();
  delay(20);

  if (!mqttClient->connected()) {
    connect();
  }

  publishTelemetry(valores_sensores());							//Leitura dos sensores e envio dos dados para a nuvem

  
  esp_deep_sleep_start();										//Sleep
}

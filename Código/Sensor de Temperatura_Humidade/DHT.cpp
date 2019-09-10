#include "DHT.h"

#define MIN_INTERVAL 2000
#define TIMEOUT -1

DHT::DHT(uint8_t pin) {
  _pin = pin;
  _maxcycles = microsecondsToClockCycles(1000);  //1 milisegundo para leitura de pulsos do sensor

}

void DHT::begin(uint8_t usec) {
  pinMode(_pin, INPUT_PULLUP);					//Configuração dos pins
  _lastreadtime = millis() - MIN_INTERVAL;
  pullTime = usec;								//=55
}

//boolean S == Scale.  True == Fahrenheit; False == Celcius
float DHT::readTemperature(bool S, bool force) {
  float f = NAN;

  if (read(force)) {
	//Shiftar dados[2] 8x à esquerda e juntar dados[3] (juntar a parte baixa com a alta)
    f = ((dados[2] & 0x7F)) << 8 | dados[3];
	//De acordo com datasheet, o valor final é a conversão de binário para decimal
	//E dividir por 10 (ou multiplicar por 0.1)
    f *= 0.1;							
	//Verificar se o bit mais significativo é 1. Caso seja, a temperatura é negativa
    if (dados[2] & 0x80) {
      f *= -1;					//Temperatura negativa
    }
  }
  return f;
}

float DHT::readHumidity(bool force) {
  float f = NAN;
  if (read(force)) {
	//Shiftar dados[0] 8x à esquerda e juntar dados[1] (juntar a parte baixa com a alta)
	f = (dados[0]) << 8 | dados[1];
	//De acordo com datasheet, o valor final é a conversão de binário para decimal
	//E dividir por 10 (ou multiplicar por 0.1)
	f *= 0.1;
  }
  return f;
}

bool DHT::read(bool force) {
  uint32_t currenttime = millis();
  if (!force && ((currenttime - _lastreadtime) < MIN_INTERVAL)) {		//Certificar que passaram os dois segundos
    return _lastresult; 												//e a leitura é bem feita
  }
  _lastreadtime = currenttime;

  //dados=0, limpar todas as posições do vetor
  dados[0]=dados[1]=dados[2]=dados[3]=dados[4]=0;
  
  //Inicio do processo de leitura
  pinMode(_pin, INPUT_PULLUP);											
  delay(1);

  //Setar a linha de dados a low por 1.1ms (1ms minimo de acordo com datasheet)
  pinMode(_pin, OUTPUT);																	//(1)
  digitalWrite(_pin, LOW);																	//(1)
  delayMicroseconds(1100); 																	//(1)
  uint32_t ciclos[80];
  {
    //Setar a linha de dados a high os 20us / 40us (de acordo com datasheet)
    pinMode(_pin, INPUT_PULLUP);															//(2)

    //Delay anterior + espera para que o sensor coloque a linha de dados a low
    delayMicroseconds(pullTime);		//pullTime=usec=55   (dht.begin)					//(2) 

    //Aqui começa a comunicação entre o esp32 e o sensor

	//Desativar as interrupções
    InterruptLock lock;

	//De acordo com datasheet, esperamos a linha de dados a low durante 80us por parte do sensor
    count_pulse_time(LOW);																		//(3)
	//Depois da linha estar a low, esperamos a mesma a high durante 80us por parte do sensor
	count_pulse_time(HIGH);																		//(4)
	
	//Ciclo para gravar count para analisar mais à frente
    for (int i=0; i<80; i+=2) {
	  //ciclos[i] guarda o count do pulso (low) que dura 50us (início de transmissão de cada bit)
      ciclos[i]   = count_pulse_time(LOW);
	  //ciclos[i+1] guarda o count do pulso (high) que pode durar entre 26/28us ou 70us
      ciclos[i+1] = count_pulse_time(HIGH);
    }
  }
  
  //count (pulso 26/28us) < count (pulso 50us) < count (pulso 70us)
  //Pulso 26/28us = bit a 0 ; Pulso 70us = bit a 1
  
  //Sempre que ciclos[i+1] > ciclos[i] temos que a linha de dados teve high mais tempo que a low
  //Assim sabemos que o tempo que esteve a high é maior do que 50us e o bit é 1
  //Sempre que ciclos[i+1] < ciclos[i] temos que a linha de dados teve high menos tempo que a low
  //Assim sabemos que o tempo que esteve a high é menor do que 50us e o bit é 0
  for (int i=0; i<40; ++i) {
    //Low nas posições pares e high nas ímpares
	uint32_t linha_low  = ciclos[2*i];
    uint32_t linha_high = ciclos[2*i+1];
	
	//Verificação de erros na contagem da função count_pulse_time
    if ((linha_low == TIMEOUT) || (linha_high == TIMEOUT)) {
      _lastresult = false;
      return _lastresult;
    }
	//Shiftar os dados
    dados[i/8] <<= 1;
	//Sempre que high>low adicionamos 0
	//1byte=1bit, dados[0] quando 0<=i<=7. Assim adicionamos 8 bits a uma posição do vetor
	//Devido à divisão de inteiros ignorar a parte decimal
    if (linha_high > linha_low) {
      dados[i/8] |= 1;
    }
  }
  //Verificação de erros finais com o CheckSum (dados[4])
  if (dados[4] == ((dados[0] + dados[1] + dados[2] + dados[3]) & 0xFF)) {
    _lastresult = true;
    return _lastresult;
  }
  else {
    _lastresult = false;
    return _lastresult;
  }
}

//Vai incrementando count enquanto o estado do pin não for alterado
//Usado para comparar os vários count de quando a linha está a low e high
//E definir se o valor do bit é 0 ou 1
uint32_t DHT::count_pulse_time(bool level) {
  uint32_t count = 0;

  while (digitalRead(_pin) == level) {
    if (count++ >= _maxcycles) {
      return TIMEOUT; 			//Excedeu o tempo estabelecido
    }
  }
  return count;
}
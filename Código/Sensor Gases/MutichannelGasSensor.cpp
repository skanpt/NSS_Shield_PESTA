/*
    MutichannelGasSensor.cpp
    2015 Copyright (c) Seeed Technology Inc.  All right reserved.

    Author: Jacky Zhang
    2015-3-17
    http://www.seeed.cc/
    modi by Jack, 2015-8

    The MIT License (MIT)

    Copyright (c) 2015 Seeed Technology Inc.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <math.h>
#include <Wire.h>
#include <Arduino.h>
#include "MutichannelGasSensor.h"

//Inicializar a libraria e fazer a comunicação I2C com o atmega168
void MutichannelGasSensor::begin(int address)
{
    r0_inited = false;
	//Inicializa a comunicação I2C com o endereço 0x04
    Wire.begin();
    i2cAddress = address;
}

//Função usada para receber dados do atmega presente no módulo com o sensor MiCS-6814, nomeadamente ADC_S
unsigned int MutichannelGasSensor::get_addr_dta(unsigned char addr_reg)
{
START:
	//Inicia a transmissão
    Wire.beginTransmission(i2cAddress);	//Início de transmissão
    Wire.write(addr_reg);				//Dados a serem enviados do master para o slave
    Wire.endTransmission();    			//Envio de dados e fim de transmissão
    
    Wire.requestFrom(i2cAddress, 2);	//Master (esp32) inicia um pedido de dados (2) ao slave (atmega168)
    
    unsigned int dta = 0;
    
    unsigned char raw[10];
    int cnt = 0;

	//Enquanto existirem dados para serem lidos (do slave para master), a função Wire.read()
	//Vai ler e guardar esses dados no vetor raw[]    
    while(Wire.available())
    {
        raw[cnt++] = Wire.read();
    }
    
    if(cnt == 0)goto START;

    dta = raw[0];
    dta <<= 8;							//Shiftar 8 à esquerda
    dta += raw[1];						//Guardar os dados de raw em dta, tudo numa só variável
    
	//Guardar o valor lido pelo ADC 
    switch(addr_reg)
    {
        case CH_VALUE_CO:
        
        if(dta > 0)
        {
            adcValueR0_CO_Buf = dta;
        }
        else 
        {
            dta = adcValueR0_CO_Buf;
        }
        
        break;
        
        case CH_VALUE_NO2:
        
        if(dta > 0)
        {
            adcValueR0_NO2_Buf = dta;
        }
        else 
        {
            dta = adcValueR0_NO2_Buf;
        }
        
        break;
        
        default:;
    }
    return dta;
}

//Função usada para receber dados do atmega presente no módulo com o sensor MiCS-6814
unsigned int MutichannelGasSensor::get_addr_dta(unsigned char addr_reg, unsigned char __dta)
{
    
START: 
    Wire.beginTransmission(i2cAddress);	//Início de transmissão
    Wire.write(addr_reg);				//Dados a serem enviados do master para o slave
    Wire.write(__dta);					//E ficam em queue até endTransmission()
    Wire.endTransmission();    			//Envio de dados e fim de transmissão
    
    Wire.requestFrom(i2cAddress, 2);	//Master (esp32) inicia um pedido de dados (2) ao slave (atmega168) 
    
    unsigned int dta = 0;
    
    unsigned char raw[10];
    int cnt = 0;
    
	//Enquanto existirem dados para serem lidos (do slave para master), a função Wire.read()
	//Vai ler e guardar esses dados no vetor raw[]
    while(Wire.available())				//Valores do vetor rcDta (no firmware do Atmega168)
    {
        raw[cnt++] = Wire.read();		//Que vão ser guardados no vetor raw
    }
    
    if(cnt == 0)goto START;
	
    dta = raw[0];						//Parte alta de ADC_0
    dta <<= 8;							//Shiftar 8 à esquerda
    dta += raw[1];						//Adicionar a parte baixa do ADC_0
    

    return dta;
}

void MutichannelGasSensor::write_i2c(unsigned char addr, unsigned char *dta, unsigned char dta_len)
{
	//Início de transmissão no endereço 0x04 default de 7 bits
    Wire.beginTransmission(addr);
	//Envia "dta_len" dta. Os dados ficam todos em espera até ser chamada a função endTransmission()
    for(int i=0; i<dta_len; i++)
    {
        Wire.write(dta[i]);
    }
	//Os dados em espera pela função write() são enviados 
	//Acaba a transmissão para que outros dispositivos em espera possam transmitir
	//Return 0 se não existirem erros
    Wire.endTransmission();
}

//Função que dá return do valor do gás pretendido, em ppm
float MutichannelGasSensor::calcGas(int gas)
{

    float ratio0, ratio1, ratio2;

	//Obter os valores dos ADC presentes no atmega 168
    int A0_1 = get_addr_dta(6, ADDR_USER_ADC_CO);			//ADC_0 do CO
    int A0_2 = get_addr_dta(6, ADDR_USER_ADC_NO2);        	//ADC_0 do NO2
	int An_1 = get_addr_dta(CH_VALUE_CO);					//ADC_S do CO
    int An_2 = get_addr_dta(CH_VALUE_NO2);					//ADC_S do NO2
	
	//Calcular a relação entre Rs e R0
    ratio1 = (float)An_1/(float)A0_1*(1023.0-A0_1)/(1023.0-An_1);		//Rs/R0 do CO, fórmula deduzida no relatório
    ratio2 = (float)An_2/(float)A0_2*(1023.0-A0_2)/(1023.0-An_2);		//Rs/R0 do NO2, fórmula deduzida no relatório
    
    float c = 0;
	
	//Equações das retas presentes no datasheet que fazem a relação
	//Rs/R0 e a concentração em partes por milhão (ppm)
    switch(gas)
    {
        case CO:
        {
            c = pow(ratio1, -1.179)*4.385;  //Expressão dada pelo fabricante, próxima do valor encontrado teóricamente (relatório)
            break;
        }
        case NO2:
        {
            c = pow(ratio2, 1.007)/6.855;  //Expressão dada pelo fabricante, próxima do valor encontrado teóricamente (relatório)
            break;
        }
        default:
            break;
    }
  
	ledOff();
    return isnan(c)?-3:c;
}

//Função que dá início ao aquecimento da resistência de aquecimento
//Resistência necessária para a existência da oxidação e desoxidação (relatório)
void MutichannelGasSensor::powerOn(void)
{
    dta_test[0] = 11;
    dta_test[1] = 1;
    write_i2c(i2cAddress, dta_test, 2);
}

MutichannelGasSensor gas;
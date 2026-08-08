#define ONE_WIRE_BUS 4
#define MH_Z19B_PWM 3 
#define CLK 2+14
#define DT 0+14
#define SW 3+14
#define _LCD_TYPE 1

#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <LCD_1602_RUS_ALL.h>
#include "GyverEncoder.h"
#include <SPI.h>          // библиотека для работы с шиной SPI 
#include "nRF24L01.h"     // библиотека радиомодуля 
#include "RF24.h"         // ещё библиотека радиомодуля 
 
RF24 radio(9, 10); 
 
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб 

int signal=0;
int AngleMin=80;
int AngleMax=155;
int AngleReal = 90;
int AngleDelta=1;

bool isHigh = false; //Высокий ли был уровень сигнала на ШИМ выходе MH-Z19B 
bool statusPWM; //Высокий ли уровень сигнала на ШИМ выходе MH-Z19B 
uint32_t tmrPWM = 0; //Таймер для измерения показателей на ШИМ выходе MH-Z19B 
int Tlow; //Продолжительность низкого сигнала на ШИМ выходе MH-Z19B 
int Thigh; //Подолжительность высокого сигнала на ШИМ выходе MH-Z19B 
int CO2; //Содержание CO2 в воздухе в ppm 
float T; //Температура воздуха
uint32_t PerData = 3.0*1000; //Время между опросами датчиков, мс
uint32_t tmrData = 0; //Таймер для опроса датчиков
int TPreheat = 30000; //Время предварительного нагрева (оптимальное - 60 секунд)
uint32_t TimerRA = -6000; //Таймер для отправки данных при поградусном повороте сервы
int CountPr = 0; //Прогресс прогрева в прямоугольниках

int Tpref = 25; //Предпочтительная температура
float K = 0.4; //Важность температуры, по отношению к CO2, k =1 - всегда препочтительней CO2
int Topening = 7; //Время открытия окна, с
int BeTime = 5; // Минимальное время между открытиями окна, минут
int Preg;

bool AutoMode=true;
bool isWOpen = false;//Открыто ли окно
bool isWOpening = false;//Открывается ли окно
bool isWClosing = false;//Закрывается ли окно
float WStart;//Время начала открытия/закрытия
float TimeLOp;//Время последнего открытия
float TimeOpenIs;//Время сколько окно уже открыто

long long tmr = 0;
long long TMR = 0;
int NuOfSl = 6;//количество слайдов
int SlNu = 1;//Номер текущего слайда
int NuOfSet = 2;//количество параметров на слайде
int SetNu = 1;//номер текущего параметра
bool update = true;//Нужно ли обновлять показ экрана
bool editing = false;//Изменяется ли сейчас параметры

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Encoder enc1(CLK, DT, SW, TYPE2);
LCD_1602_RUS lcd(0x27, 16, 2);

bool decision(){
  if(T>Tpref){
    return true;
  }else if(K==1){
    return false;
  }else if(CO2-400 > K*(Tpref-T)*(Tpref-T)*150.0){
    return true;
  }else{
    return false;
  }
}

void setup() {
  TimeLOp=millis();
  Serial.begin(9600);
  sensors.begin(); 

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  pinMode(MH_Z19B_PWM, INPUT); 

  radio.begin();              // активировать модуль 
  radio.setAutoAck(1);        // режим подтверждения приёма, 1 вкл 0 выкл 
  radio.setRetries(0, 15);    // (время между попыткой достучаться, число попыток) 
  radio.enableAckPayload(); 
  radio.setPayloadSize(1);   // размер пакета, в байтах 
 
  radio.openWritingPipe(address[0]);  // мы - труба 0, открываем канал для передачи данных 
  radio.setChannel(0x6a);             // выбираем канал (в котором нет шумов!) 
 
  radio.setPALevel (RF24_PA_LOW);   // уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX 
  radio.setDataRate (RF24_250KBPS); // скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS 
  //должна быть одинакова на приёмнике и передатчике! 
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!! 
 
  radio.powerUp();        // начать работу 
  radio.stopListening();  // не слушаем радиоэфир, мы передатчик 
}

void loop() {
  enc1.tick();
  statusPWM = digitalRead(MH_Z19B_PWM); 
  if (statusPWM && !isHigh) {   // LOW-HIGH 
    isHigh = true; 
    Tlow = millis() - tmrPWM; 
    tmrPWM = millis(); 
  }else if (!statusPWM && isHigh) {   // HIGH-LOW 
    isHigh = false; 
    Thigh = millis() - tmrPWM; 
    tmrPWM = millis(); 
    CO2 = 5000*((Thigh-2)*1.0/(Thigh+Tlow-4)); 
  }       
  if((millis()-tmrData >= PerData)&&SlNu==1){ 
    //Serial.print(String(millis()/(1000.0)) + "\t"); //Время в сеундах, дробное 
    if(millis() - tmrPWM > 10000){ 
      Serial.print(String("Ошибка") + "\t"); 
    }else{ 
      //Serial.print(String(CO2) + "\t"); 
    } 
    sensors.requestTemperatures();
    T = sensors.getTempCByIndex(0);
    //Serial.println(T); 
    tmrData = millis(); 
    update=true;
  } 
//Открывается ли или закрывается ли окно
  if(isWOpening==1){
    if((millis()-WStart)>=Topening*1000){
      isWOpening=false;
      isWOpen = true;
      update=true;
      TimeOpenIs = millis();
    }
  }else if(isWClosing==1){
    if((millis()-WStart)>=Topening*1000){
      isWClosing=false;
      isWOpen = false;
      update=true;
    }
    TimeLOp=millis();
  }

  if((AutoMode==1)&&(decision()==1)&&(isWOpen==0)&&(isWOpening==0)&&(isWClosing==0)&&((millis()-TimeLOp)/1000.0>=(BeTime*60))){
    Serial.println((millis()-TimeLOp)/1000.0);
    Serial.println(">");
    Serial.println(BeTime*60);
    isWOpening=1;
    WStart=millis();
    update=true;
  }else if((millis()-TimeOpenIs>=Topening*1000)&&(isWOpen==1)&&(isWClosing==0)&&(AutoMode==1)){
    isWClosing=1;
    WStart=millis();
    update=true;
  }
  if(enc1.isRelease()&SlNu!=1){//отпустили кнопку
    editing = !editing;
    Serial.println("!editing");
    update = true;
  }
  if(!editing){//меняем слайды
    if (enc1.isRight()){
      Serial.println("SL+");
      SlNu = SlNu+1;
      update = true;
    }else if (enc1.isLeft()){
      Serial.println("SL-");
      SlNu = SlNu-1;
      update = true;
    }if(SlNu > NuOfSl){
      SlNu = 1;
    }else if(SlNu < 1){
      SlNu = NuOfSl;
    }
  }else{//меняем настройки
    if (enc1.isRight()){
      SetNu = SetNu+1;
      update = true;
    }if (enc1.isLeft()){
      SetNu = SetNu-1;
      update = true;
    }if(SetNu > NuOfSet){
      SetNu = 1;
    }else if(SetNu < 1){
      SetNu = 2;
    }

    if (enc1.isRightH()){
      update = true;
      tmr = 0;
      if(SlNu == 2){
        if(SetNu == 1){
          Tpref=Tpref+1;
          if(Tpref > 35){
            Tpref = 35;
            update = false;
          }
        }else{
          K=K+0.05;
          if(K > 1){
            K = 1;
            update = false;
          }
        }
      }else if(SlNu == 3){
        if(SetNu == 1){
          Topening = Topening+1;
          if(Topening > 999){
            Topening = 999;
            update = false;
          }
        }else{
          BeTime = BeTime+1;
          if(BeTime > 999){
            BeTime = 999;
            update = false;
          }
        }
      }else if(SlNu == 4){
        if(SetNu == 1){
          if(AutoMode==1){
            AutoMode=0;
          }else{
            AutoMode=1;
          }
        }else{
          if(isWOpen==1){
          isWClosing = true;
          WStart=millis();
          }else if(isWOpen==0){
            isWOpening = true;
            WStart=millis();
          }
        }
      }
      else if(SlNu == 5){
        if(SetNu == 1){
          AngleMin=AngleMin+AngleDelta;
          if(AngleMin>=180){
            AngleMin=180;
          }
        }else{
          AngleMax=AngleMax+AngleDelta;
          if(AngleMax>=180){
            AngleMax=180;
          }
        }
      }
      else if(SlNu == 6){
        if((SetNu == 1)&&(!isWOpening&&!isWClosing)){
          AngleReal=AngleReal+AngleDelta;
          if(AngleReal>=180){
            AngleReal=180;
          }
          TimerRA = millis();
        }else{
          AngleDelta++;
          if(AngleDelta>=180){
            AngleDelta=180;
          }
        }
      }
    }else if (enc1.isLeftH()){
      update = true;
      tmr = -1000;
      if(SlNu == 2){
        if(SetNu == 1){
          Tpref=Tpref-1;
          if(Tpref < -35){
            Tpref = -35;
            update = false;
          }
        }else{
          K=K-0.05;
          if(K < 0.0){
           K = 0.0;
           update = false;
          }
        }
      }else if(SlNu == 3){
        if(SetNu == 1){
          Topening = Topening-1;
          if(Topening <1){
            Topening = 1;
            update = false;
          }
        }else{
          BeTime = BeTime-1;
          if(BeTime <0){
            BeTime = 1;
            update = false;
          }
        }
      }else if(SlNu == 4){
        if(SetNu == 1){
          if(AutoMode==1){
            AutoMode=0;
          }else{
            AutoMode=1;
          }
        }else if((SetNu==2)&&(AutoMode==0)){
          if(isWOpen==1){
            isWClosing = true;
            WStart=millis();
          }else if(isWOpen==0){
            isWOpening = true;
            WStart=millis();
          }
        }
      }
      else if(SlNu == 5){
        if(SetNu == 1){
          AngleMin=AngleMin-AngleDelta;
          if(AngleMin<=0){
            AngleMin=0;
          }
        }else{
          AngleMax=AngleMax-AngleDelta;
          if(AngleMax<=0){
            AngleMax=0;
          }
        }
      }else if(SlNu == 6){
        if((SetNu == 1)&&(!isWOpening&&!isWClosing)){
          AngleReal=AngleReal-AngleDelta;
          if(AngleReal<=0){
            AngleReal=0;
          }
          TimerRA = millis();
        }else{
          AngleDelta--;
          if(AngleDelta<=0){
            AngleDelta=0;
          }
        }
        
      }
    }
  }
  if(update){//Обновление дисплея
    Serial.println("update");
    lcd.clear();
    lcd.setCursor(0,0);
    if(SlNu==1){
      if(millis()<TPreheat){
        lcd.print("Preheat sensors");
      }else{
        lcd.print("CO2 " + String(CO2)); lcd.print("  T " + String(T,1));
      }
      lcd.setCursor(0,1); 
      if(isWOpening==1){
        lcd.print("Window opening");
      }else if(isWClosing==1){
        lcd.print("Window closing");
      }else{
        if(isWOpen==1){
          lcd.print("Window is open");
        }else if(isWOpen==0){
          lcd.print("Window is closed");
        }
      }
    }else if(SlNu==2){
      if(editing && SetNu==1){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Tpref      "); lcd.print(String(Tpref) + "°C");
      lcd.setCursor(0,1); 
      if(editing && SetNu==2){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Imp T  ");
      char S[4];
      dtostrf(K, 6, 2, S);
      lcd.print("  "); lcd.print(S);
    }else if(SlNu==3){
      if(editing && SetNu==1){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Op time "); lcd.print(String(Topening) + "S");
      lcd.setCursor(0,1); 
      if(editing && SetNu==2){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Time between " + String(BeTime)); lcd.print("m");
    }else if(SlNu==4){
      if((editing && SetNu==1)||((AutoMode==1)&&editing)){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Auto mode ");
      if(AutoMode==1){
        lcd.print("on");
      }else{
        lcd.print("off");
      }
      lcd.setCursor(0,1); 
      if(AutoMode==0){
        if(editing && SetNu==2){
          lcd.print(">");
        }else if(editing){
          lcd.print(" ");
        }
        if(isWOpen==1){
          lcd.print("Close window? ");
          if(isWClosing==1){
            lcd.print("ye");
          }else{
            lcd.print("no");
          }
        }else if(isWOpen==0){
          lcd.print("Open window? ");
          if(isWOpening==1){
            lcd.print("yes");
          }else{
            lcd.print("no");
          }
        }
      }else{
        lcd.print("                 ");
      }
    }
    else if(SlNu==5){
      if(editing && SetNu==1){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Angle min ");
      lcd.print(String(AngleMin));
      lcd.setCursor(0,1); 
      if(editing && SetNu==2){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Angle max ");
      lcd.print(String(AngleMax));
    }
    else if(SlNu==6){
      if(editing && SetNu==1){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("Real Angle ");
      lcd.print(String(AngleReal));
      lcd.setCursor(0,1); 
      if(editing && SetNu==2){
        lcd.print(">");
      }else if(editing){
        lcd.print(" ");
      }
      lcd.print("AngleDelta ");
      lcd.print(String(AngleDelta));
    }
    update = false;
  }
  if(isWOpening){
    signal=AngleMin;
    radio.write(&signal, 1); 
    Serial.println(AngleMin);
  }
  else if(isWClosing){
    signal=AngleMax;
    radio.write(&signal, 1); 
    Serial.println(AngleMax);
  }else if ((millis()-TimerRA < 1000)&&(millis()-TimerRA > 400)){
    signal=AngleReal;
    radio.write(&signal, 1);
    Serial.println(AngleReal);
  }
}
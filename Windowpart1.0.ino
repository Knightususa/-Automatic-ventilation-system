#define CLK 2+14
#define DT 0+14
#define SW 3+14

#include <Servo.h>
#include <SPI.h>          // библиотека для работы с шиной SPI 
#include "nRF24L01.h"     // библиотека радиомодуля 
#include "RF24.h"         // ещё библиотека радиомодуля 

byte pipeNo, gotByte; 
int Target = 90, Prev = 90, Signal = 0;
bool isMove = 0;

RF24 radio(9, 10); // "создать" модуль на пинах 9 и 10 Для Уно и Нано
Servo servo;

byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб 
byte recieved_data[2];

void SlowMotion(int a, int b, int sp){
  isMove = 1;
  int x;
  if(b>a){
    x=1;
  }else{
    x=-1;
  }
  for(int i = a; i != b; i=i+x){
    servo.write(i);
    delay(1000/sp);
  }
  isMove = 0;
}

void setup() {
  radio.begin();              // активировать модуль
  radio.setAutoAck(1);        // режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(0, 15);    // (время между попыткой достучаться, число попыток)
  radio.setPayloadSize(1);   // размер пакета, в байтах

  radio.openReadingPipe(1, address[0]);   // хотим слушать трубу 0
  radio.setChannel(0x6a);     // выбираем канал (в котором нет шумов!)

  radio.setPALevel (RF24_PA_LOW);   // уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate (RF24_250KBPS); // скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!

  radio.powerUp();        // начать работу
  radio.startListening(); // начинаем слушать эфир, мы приёмный модуль
  Serial.begin(9600);

  servo.attach(2);
  servo.write((((90+48.5)*65.0)/90));
}

void loop() {
  while (radio.available(&pipeNo)) {        // слушаем эфир со всех труб 
    radio.read(&gotByte, 1);  // читаем входящий сигнал 
  } 

  if((int(gotByte) != Target)&&(int(gotByte) != 0)){
    Prev = Target;
    Target = int(gotByte);
    SlowMotion((((Prev+48.5)*65.0)/90), (((Target+48.5)*65.0)/90), 20);
    Serial.print("SlMo");
    Serial.print(Prev);
    Serial.println(Target);
  }  
}

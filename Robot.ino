#include <SoftwareSerial.h>
#define ROTATION_LEFT 4 //направление вращения левого колеса
#define VOLTAGE_LEFT 5 //управляющее напряжение левого колеса
#define VOLTAGE_RIGHT 6 //управляющее напряжение правого колеса
#define ROTATION_RIGHT 7 //направление вращения правого колеса

#define debug 0

SoftwareSerial mySerial(9, 8);

float speed = 0; //требуемая скорость (см/с)

///////////////////////////////////////////////////////////////////////////////
///////////   Счетчик прерываний энкодера /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
unsigned long int left_cnt, left_cnt0 = 0; 
unsigned long int right_cnt, right_cnt0 = 0; 
unsigned long int time_left0 = 0; 
unsigned long int time_right0 = 0;

//подсчет отверстий на энкодере левого колеса
void left_wheel(void)
{
  unsigned long int time_left = micros();
  unsigned long int dt_left = time_left - time_left0;
  time_left0 = time_left;

  if (dt_left > 100)
    left_cnt++;
}

//подсчет отверстий на энкодере правого колеса
void right_wheel(void)
{
  unsigned long int time_right = micros();
  unsigned long int dt_right = time_right - time_right0;
  time_right0 = time_right;

  if (dt_right > 100)
    right_cnt++;
}
///////////////////////////////////////////////////////////////////////////////

unsigned long int old_time = 0;

void setup() 
{
  pinMode(9, INPUT);
  pinMode(8, OUTPUT);
  mySerial.begin(9600); // BT serial
  Serial.begin(9600);

  pinMode(ROTATION_LEFT, OUTPUT);
  pinMode(VOLTAGE_LEFT, OUTPUT);
  pinMode(VOLTAGE_RIGHT, OUTPUT);
  pinMode(ROTATION_RIGHT, OUTPUT);
  
  digitalWrite(ROTATION_LEFT, HIGH);
  digitalWrite(VOLTAGE_LEFT, LOW);
  digitalWrite(VOLTAGE_RIGHT, LOW);
  digitalWrite(ROTATION_RIGHT, HIGH);

  attachInterrupt(digitalPinToInterrupt(3),left_wheel,CHANGE); //обработчик прерываний левого колеса
  attachInterrupt(digitalPinToInterrupt(2),right_wheel,CHANGE); //обработчик прерываний правого колеса
 
  old_time = micros();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//Управление с помощью кнопок по BT

float voltage_left = 0; 
float voltage_right = 0; //управляющее напряжение

void remote_control(void)
{
  if (mySerial.available() > 0)
  {
    char cmd = mySerial.read();
    Serial.print((char)cmd); // debug

    if (cmd == '0')
    {
      Serial.println(" Start");
      speed = 0.06;
    }
    else if (cmd == 'X')
    {
      Serial.println(" Stop");
      voltage_left = 0;
      voltage_right = 0;
      speed = 0;
    }
    else if (cmd == 'F')
    {
      Serial.println(" Вперёд");
      digitalWrite(ROTATION_LEFT, HIGH);
      digitalWrite(ROTATION_RIGHT, HIGH);
    }  
    else if (cmd == 'B')
    {
      Serial.println(" Назад");
      digitalWrite(ROTATION_LEFT, LOW);
      digitalWrite(ROTATION_RIGHT, LOW);
    }  
    else if (cmd == 'L')
    {
      Serial.println(" Налево");
      left_cnt += 10; //60
    }
    else if (cmd == 'R')
    {
      Serial.println(" Направо");
      right_cnt += 10; //30
    }
    else if (cmd == 'Q')
    {
      Serial.println(" Быстрее");
      speed *= 1.1; //1.05
      Serial.print("Speed = "); 
      Serial.println(speed);
    }
    else if (cmd == 'S')
    {
      Serial.println(" Медленнее");
      speed /= 1.1; //1.05
      Serial.print("Speed = "); 
      Serial.println(speed);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

long int error_S = 0;
float error_V_left = 0;
float error_V_right = 0;
float error_V_left_new = 0;
float error_V_right_new = 0;
float d_error_V_left = 0;
float d_error_V_right = 0;
float a_left; //ускорение левого
float a_right; //ускорение правого
unsigned long int dtime;
unsigned long int time;

void error_calc(void)
{
  float dcnt_left = left_cnt - left_cnt0;
  left_cnt0 = left_cnt;

  float dcnt_right = right_cnt - right_cnt0;
  right_cnt0 = right_cnt;

  float S_left = 6*3.14*(dcnt_left/40); 
  float S_right = 6*3.14*(dcnt_right/40);

  float V_left = S_left/dtime*1000*0.99; //...*0.99
  float V_right = S_right/dtime*1000;

  error_S = left_cnt - right_cnt;

  error_V_left = 0.7*error_V_left + 0.3*(V_left - speed);
  d_error_V_left = error_V_left - error_V_left_new;
  error_V_left_new = error_V_left;

  error_V_right = 0.7*error_V_right + 0.3*(V_right - speed);
  d_error_V_right = error_V_right - error_V_right_new;
  error_V_right_new = error_V_right;

  a_left = d_error_V_left/dtime;
  /*if (A > A_max) 
    A_max = A;
  if (A < A_min) 
    A_min = A;*/

  a_right = d_error_V_right/dtime;
  /*if (B > B_max) 
    B_max = B;
  if (B < B_min) 
    B_min = B; */ 
  
  if(0){
    /*Serial.print ("A_max = ");
    Serial.print (A_max*100000000,8);
    Serial.print (", A_min = ");
    Serial.print (A_min*100000000,8);
    Serial.print (", B_max = ");
    Serial.print (B_max*100000000,8);
    Serial.print (", B_min = ");
    Serial.print (B_min*100000000,8);*/
    Serial.print (", error_V_left = ");
    Serial.print (error_V_left);
    Serial.print (", error_V_right = ");
    Serial.println (error_V_right);
  }

}
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

float KP = 700; //коэффициент пропорционального регулятора (700)
float KI = 0.25; //коэффициент интегрального регулятора
float KD = 0.5; //коэффициент дифференциального регулятора
int N = 0;
void control(void)
{
  if (speed<0.01){
    voltage_left = 0;
    voltage_right = 0;
    //voltage_left -= (error_S*KI - (a_left*KD));
    //voltage_right -= (-error_S*KI - (a_right*KD));
  }
  else
  {
    voltage_left -= (error_V_left*KP + error_S*KI - (a_left*KD));
    voltage_right -= (error_V_right*KP - error_S*KI - (a_right*KD));
  }
  N += 1;

  if (voltage_left > 255)  
    voltage_left = 255;
  if (voltage_right > 255) 
    voltage_right = 255;
  if (voltage_left < 0)    
    voltage_left = 0;
  if (voltage_right < 0)   
    voltage_right = 0;

  analogWrite(VOLTAGE_RIGHT, round(voltage_right));
  analogWrite(VOLTAGE_LEFT, round(voltage_left));

  if(N > 100){
    Serial.print ("voltage_left = ");
    Serial.print (voltage_left);
    Serial.print (", voltage_right = ");
    Serial.print (voltage_right);

    Serial.print (", error_S = ");
    Serial.println (error_S);
    /*Serial.println (N);
    Serial.print ("A = ");
    Serial.print (A);
    Serial.print (", B = ");
    Serial.print (B);
    Serial.print (", A*KD = ");
    Serial.print (A*KD);
    Serial.print (", B*KD = ");
    Serial.println (B*KD);*/
    N = 0;
  }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void loop() 
{
  time = micros();
  dtime = time - old_time;

  if(dtime > 10000) // timer
  {
    old_time = time;

    remote_control();
    error_calc();
    control();
  }
}

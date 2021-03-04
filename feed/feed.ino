#include <avr/pgmspace.h>
#include <Wire.h>

// ***** LCD *****
#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);
char LCD_Row_1[17];
char LCD_Row_2[17];


// ***** Stepper Motor *****
#define MotorPort                    PORTL                   
#define MotorInitialization()        DDRL=B11111111          
#define Motor_X_SetPulse()           MotorPort &= ~(1<<0)     
#define Motor_X_RemovePulse()        MotorPort |= (1<<0)      
#define Motor_X_Forward()            MotorPort |= (1<<2)      
#define Motor_X_Reverse()            MotorPort &= ~(1<<2)     
#define Motor_X_Enable()             MotorPort |= (1<<4)      
#define Motor_X_Disable()            MotorPort &= ~(1<<4)     
boolean Step_On_flag=false;                                  
boolean Mode_On_flag=false;                                  


// ***** Taho *****
#define TahoPort                      PORTL
#define TahoSetPulse()                TahoPort |= (1<<6)             
#define TahoRemovePulse()             TahoPort &= ~(1<<6)            


// ***** Encoder *****
#define Enc_Line_per_Revolution       2400                           
#define Enc_Line                      Enc_Line_per_Revolution*2      
#define EncoderPort                   PORTD
#define EncoderInitialization()       DDRD=B00000000                
#define Enc_Read()                    (PIND & B00000010)
volatile int Enc_Pos=0;                                             
volatile int Ks_Count=0;
volatile int Km_Count=0;
int Ks_Divisor=0;
int Km_Divisor=0;
int Enc_Pos_tmp=0;
long Spindle_Angle=0;


//***** Sensor *****
#define SensorPort                     PORTD
#define Sensor                         PIND
#define Sensor_Left_Mask B00001000
#define Sensor_Right_Mask B00000100
char Sensor_Mask = B00000000;


enum Pressed_Key
{
  Key_None,
  Key_Right,
  Key_Up,
  Key_Down,
  Key_Left,
  Key_Select
};
byte Pressed_Key=Key_None;
boolean key_flag=false;                                            


// ***** Mode *****
enum Mode
{
  Mode_Thread_Right=1,
  Mode_Feed_Left,
  Mode_Divider,
  Mode_Feed_Right,
  Mode_Thread_Left
};
byte Mode = Mode_Feed_Left;


// ***** Feeds *****
#define Total_Feeds                   25                 
typedef struct
{
  int  s_Divisor;                                      
  char  Text[7];
}
FEED_INFO;
FEED_INFO Feed_Info[Total_Feeds] =
{
   { 900, "0.02mm" },
   { 450,  "0.04mm" },
   { 300,  "0.06mm" },
   { 225,  "0.08mm" },
   { 180,  "0.10mm" },
   { 150,  "0.12mm" },
   { 129,  "0.14mm" },
   { 112,  "0.16mm" },
   { 100,  "0.18mm" },
   { 90,  "0.20mm" },
   { 60, "0.30mm" },
   { 45,  "0.40mm" },
   { 36,  "0.50mm" },
   { 30,  "0.60mm" },
   { 26,  "0.70mm" },
   { 23,  "0.80mm" },
   { 20,  "0.90mm" },
   { 18,  "1.00mm" },
   { 16,  "1.10mm" },
   { 15,  "1.20mm" },
   { 14,  "1.30mm" },
   { 13,  "1.40mm" },
   { 12,  "1.50mm" },
   { 11,  "1.60mm" },
   { 10,  "1.70mm" },
};
byte Feed_Step = 4;                                      


// ***** Threads *****
#define Total_Threads                 26               
typedef struct
{
  int s_Divisor;                                       
  int  m_Divisor;                                      
  char Text[7];
}
THREAD_INFO;
THREAD_INFO Thread_Info[Total_Threads] =
{
   { 90, 0, "0.20mm" },
   { 72,  0,    "0.25mm" },
   { 60,  0, "0.30mm" },
   { 51,  4286, "0.35mm" },
   { 45,  0, "0.40mm" },
   { 36,  0, "0.50mm" },
   { 30,  0, "0.60mm" },
   { 25,  7143, "0.70mm" },
   { 24,  0,    "0.75mm" },
   { 22,  5000, "0.80mm" },
   { 18,  0, "1.00mm" },
   { 14,  4000, "1.25mm" },
   { 12,  0, "1.50mm" },
   { 10,  2857, "1.75mm" },
   { 9,  0, "2.00mm" },
   { 45, 0, "64tpi " },
   { 40, 0, "56tpi " },
   { 33, 9622, "48tpi " },
   { 28, 1250, "40tpi " },
   { 22, 7848, "32tpi " },
   { 16, 9811, "24tpi " },
   { 14, 1732, "20tpi " },
   { 12, 7659, "18tpi " },
   { 11, 3207, "16tpi " },
   { 9, 9447, "14tpi " },
   { 8, 4905, "12tpi " },
};
byte Thread_Step = 10;                     


// ***** Interrupts *****
#define EnableINT0()          EIMSK |= (1 << INT0)
#define DisableINT0()         EIMSK &= ~(1 << INT0)
#define Init_INT0_Any()       EICRA = B00000001


//***************************************************************
void setup()
{
  TIMSK0=0;
  
  EncoderInitialization();
  PORTD=B00001111;
  
  MotorInitialization();
  Init_INT0_Any();
  EnableINT0();
  
  lcd.begin(16, 2);
}


//****************************************************************
void loop()
{
  Enc_Pos_tmp = Enc_Pos;

  if ((Mode == Mode_Divider) || !Mode_On_flag)
  {
    Motor_X_Disable();
  }
  else
  {
    Motor_X_Enable();
  }
  
  menu();
  Sensors();
}


//******************************************************************
void menu()
{
  int ADC_value = analogRead(A0);
  if (ADC_value < 65) Pressed_Key=Key_Right;
  else if (ADC_value < 220) Pressed_Key=Key_Up;
  else if (ADC_value < 390) Pressed_Key=Key_Down;
  else if (ADC_value < 600) Pressed_Key=Key_Left;
  else if (ADC_value < 870) Pressed_Key=Key_Select;
  else Pressed_Key = Key_None;
  
  if (!key_flag)
  {
      switch (Pressed_Key)
      {
        case Key_Left:
          MenuKeyLeftPressed();
          break;
        case Key_Right:
          MenuKeyRightPressed();
          break;
        case Key_Up:
          MenuKeyUpPressed();
          break;
        case Key_Down:
          MenuKeyDownPressed();
          break;       
        case Key_Select:
          MenuKeySelectPressed();    
          break;
      }
  }
  if (Pressed_Key == Key_None) key_flag = false;

  SelectWorkMode();
}

//******************************************************************
void MenuKeySelectPressed()
{
  switch (Mode) 
  {
      case Mode_Thread_Right:
      case Mode_Thread_Left:
      case Mode_Feed_Left:
      case Mode_Feed_Right:
        Step_On_flag = false;
        Mode_On_flag = !Mode_On_flag;
        Ks_Count = 0;
        Km_Count = 0;
        break;
      case Mode_Divider:
        Enc_Pos=0;
        break;
   }
   key_flag = true;
}

//******************************************************************
void MenuKeyUpPressed()
{
  switch (Mode)
  {
      case Mode_Thread_Right:
      case Mode_Thread_Left:
        if (Thread_Step < Total_Threads-1)
        {
          Mode_On_flag = false;
          Step_On_flag = false;
          Ks_Count=0;
          Km_Count=0;
          Thread_Step++;
        }
        break;
      case Mode_Feed_Left:
      case Mode_Feed_Right:
        if (Feed_Step < Total_Feeds-1)
        {
          Ks_Count=0;
          Feed_Step++;
        }
        break;
      }
      key_flag = true;
}

//******************************************************************
void MenuKeyDownPressed()
{
   switch (Mode)
   { 
      case Mode_Thread_Right:
      case Mode_Thread_Left:
        if (Thread_Step > 0)
        {
          Mode_On_flag = false;
          Step_On_flag = false;
          Ks_Count=0;
          Km_Count=0;
          Thread_Step--;
        }
        break;
      case Mode_Feed_Left:
      case Mode_Feed_Right:
        if (Feed_Step > 0)
        {
          Ks_Count=0;
          Feed_Step--;
        }
        break;
   }
   key_flag = true;
}

//******************************************************************
void MenuKeyLeftPressed()
{
      switch (Mode)
      {
        case Mode_Feed_Left:
        case Mode_Divider:
        case Mode_Feed_Right:
        case Mode_Thread_Left:
          Mode_On_flag = false;
          Step_On_flag = false;
          Ks_Count=0;
          Km_Count=0;
          Mode--;
          break;
      }
      
      key_flag = true;
}

//******************************************************************
void MenuKeyRightPressed()
{
      switch (Mode)
      {
        case Mode_Thread_Right:
        case Mode_Feed_Left:
        case Mode_Divider:
        case Mode_Feed_Right:
          Mode_On_flag = false;
          Step_On_flag = false;
          Ks_Count=0;
          Km_Count=0;
          Mode++;
          break;
      }
      
      key_flag = true;
}

//******************************************************************
void SelectWorkMode()
{
   switch (Mode)
   {
    case Mode_Thread_Left:
        Thread_Left();
        break;
    case Mode_Feed_Left:
        Motor_X_Reverse();
        Feed_Left();
        break;
    case Mode_Divider:
        Divider();
        break;
    case Mode_Feed_Right:
        Motor_X_Forward();
        Feed_Right();
        break;
    case Mode_Thread_Right:
        Thread_Right();
  }
}


//******************************************************************
void Thread_Left()
{
  Sensor_Mask = Sensor_Left_Mask;
  Ks_Divisor=Thread_Info[Thread_Step].s_Divisor;
  Km_Divisor=Thread_Info[Thread_Step].m_Divisor;
  snprintf(LCD_Row_1, 17, (Mode_On_flag==true) ? "Mode:  ON       " : "Mode:  OFF      ");
  snprintf(LCD_Row_2, 17, "Thread => %s", Thread_Info[Thread_Step].Text);
  Print();
}

void Feed_Left()
{
  Sensor_Mask = Sensor_Left_Mask;
  Ks_Divisor=Feed_Info[Feed_Step].s_Divisor;
  snprintf(LCD_Row_1, 17, (Mode_On_flag==true) ? "Mode:  ON       " : "Mode:  OFF      ");
  snprintf(LCD_Row_2, 17, "Feed   <= %s", Feed_Info[Feed_Step].Text);
  Print();
}

void Divider()
{
  if (Enc_Pos == Enc_Pos_tmp)
  {
    Spindle_Angle=(Enc_Pos*360000/(Enc_Line));
  }
  snprintf(LCD_Row_1, 17, "Mode:  Divider  ");
  snprintf(LCD_Row_2, 17, "Angle: %3ld.%03ld  ", Spindle_Angle/1000, Spindle_Angle%1000);
  Print();
}

void Feed_Right()
{
  Sensor_Mask = Sensor_Right_Mask;
  Ks_Divisor=Feed_Info[Feed_Step].s_Divisor;
  snprintf(LCD_Row_1, 17, (Mode_On_flag==true) ? "Mode:  ON       " : "Mode:  OFF      ");
  snprintf(LCD_Row_2, 17, "Feed   => %s", Feed_Info[Feed_Step].Text);
  Print();
}

void Thread_Right()
{
  Sensor_Mask = Sensor_Right_Mask;
  Ks_Divisor=Thread_Info[Thread_Step].s_Divisor;
  Km_Divisor=Thread_Info[Thread_Step].m_Divisor;
  snprintf(LCD_Row_1, 17, (Mode_On_flag==true) ? "Mode:  ON       " : "Mode:  OFF      ");
  snprintf(LCD_Row_2, 17, "Thread <= %s", Thread_Info[Thread_Step].Text);
  Print();
}


//******************************************************************
void Print()
{
lcd.setCursor(0, 0);
lcd.print(LCD_Row_1);
lcd.setCursor(0, 1);
lcd.print(LCD_Row_2);
}


//******************************************************************
void Sensors()
{
   if (!(Sensor & Sensor_Mask) )
   {
      Mode_On_flag = false;
   }
}


//******************************************************************
ISR(INT0_vect)
{
   TahoRemovePulse();
   Motor_X_RemovePulse();

   if (!Enc_Read())
   {
      Enc_Pos++;
      if (Enc_Pos == Enc_Line)
      {                                           
         Enc_Pos = 0;
         TahoSetPulse();
         Step_On_flag = (Mode_On_flag == true);
      }
      
                 
      if (Step_On_flag)
      {
         if (!(Sensor & Sensor_Mask) )
         {
            Step_On_flag = false;
            return;
         }
        
         if (Mode == Mode_Feed_Left)
         {
               if (Ks_Count == Ks_Divisor)
               {
                  Motor_X_SetPulse();
                  Ks_Count = 0;
               }
               Ks_Count++;
         }
         
         else if (Mode == Mode_Feed_Right)
         {
               if (Ks_Count == Ks_Divisor)
               {
                  Motor_X_SetPulse();
                  Ks_Count = 0;
               }
               Ks_Count++;
         }
         
         else if (Mode == Mode_Thread_Left)
         {
               if (Ks_Count > Ks_Divisor)
               {
                  Motor_X_Reverse();
                  Motor_X_SetPulse();
                  Km_Count = Km_Count + Km_Divisor;
                  if (Km_Count > Km_Divisor)
                  {
                     Km_Count = Km_Count - 10000;
                     Ks_Count = 0;
                  }
                  else
                  {
                  Ks_Count = 1;
                  }
               }
               Ks_Count++;
         }
         
         else if (Mode == Mode_Thread_Right)
         {
               if (Ks_Count > Ks_Divisor)
               {
                  Motor_X_Forward();
                  Motor_X_SetPulse();
                  Km_Count = Km_Count + Km_Divisor;
                  if (Km_Count > Km_Divisor)
                  {
                     Km_Count = Km_Count - 10000;
                     Ks_Count = 0;
                  }
                  else
                  {
                  Ks_Count = 1;
                  }
               }
               Ks_Count++;
         }
      }   
   }


   else
   {
      Enc_Pos--;
      if (Enc_Pos < 0)
      {
         Enc_Pos = Enc_Line - 1;
         TahoSetPulse();
         Step_On_flag = (Mode_On_flag == true);
      }

      if (Step_On_flag)
      {
         if (!(Sensor & Sensor_Mask) )
         {
            Step_On_flag = false;
            return;
         }
         
         if (Mode == Mode_Feed_Left)
         {
               if (Ks_Count == 0)
               {
                  Motor_X_SetPulse();
                  Ks_Count = Ks_Divisor;
               }
               Ks_Count--;
         }
         
         if (Mode == Mode_Feed_Right)
         {
               if (Ks_Count == 0)
               {
                  Motor_X_SetPulse();
                  Ks_Count = Ks_Divisor;
               }
               Ks_Count--;
         }
         
         if (Mode == Mode_Thread_Left)
         {
               if (Ks_Count == 0)
               {
                  Motor_X_Forward();
                  Motor_X_SetPulse();
                  Km_Count = Km_Count - Km_Divisor;
                  if (Km_Count < 1)
                  {
                     Km_Count = Km_Count + 10000;
                     Ks_Count = Ks_Divisor + 1;
                  }
                  else
                  {
                     Ks_Count = Ks_Divisor;
                  }
               }
               Ks_Count--;
         }
         
         if (Mode == Mode_Thread_Right)
         {
               if (Ks_Count == 0)
               {
                  Motor_X_Reverse();
                  Motor_X_SetPulse();
                  Km_Count = Km_Count - Km_Divisor;
                  if (Km_Count < 1)
                  {
                     Km_Count = Km_Count + 10000;
                     Ks_Count = Ks_Divisor + 1;
                  }
                  else
                  {
                     Ks_Count = Ks_Divisor;
                  }
               }
               Ks_Count--;
         }
      }
   }
}

//*******************************************************************

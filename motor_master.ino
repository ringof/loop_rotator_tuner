 /* remote antenna tuner, wireless motor test based on
  * track pacer node
  (c) 2020 Lyle Hazel, Len Long, Howard Mintz 
  see LICENSE.txt */

#define     MOTOR_POWER_PIN    4
#define     MOTOR_POWER_ON     LOW
#define     MOTOR_POWER_OFF   HIGH

#define     SPEED_PIN         5
#define     SPEED_FAST        LOW
#define     SPEED_SLOW       HIGH

#define     DIRECTION_LEFT_PIN     6
#define     DIRECTION_RIGHT_PIN    7
#define     DIRECTION_OFF     HIGH
#define     DIRECTION_PRESSED LOW

#define     MOTOR_SELECT_PIN    8
#define     MOTOR_SELECT_A     LOW
#define     MOTOR_SELECT_B    HIGH

#define     SPEED_FAST_CHAR     '7'
#define     SPEED_SLOW_CHAR     '2'

#define T0_COUNTS_PER_SECOND  (125*125)
void timer0_setup(void)
{
  TCCR0A = 0;// (1 << COM0A1) | (1 << COM0A0) |
          // (1 << COM0B1) | (1 << COM0B0) |
          // (1 << WGM01) | (1 << WGM00);
  TCCR0B = // (1 << FOC0A) | (1 << FOC0B) | (1 << WGM02)
          (1 << CS02) | (1 << CS00);  // Clk / 1024
  TCNT0 = 0;
}
#define DEBOUNCE_TICKS (T0_COUNTS_PER_SECOND / 10)

void setup()
{
    Serial.begin(9600);
    pinMode(MOTOR_SELECT_PIN, INPUT);
    pinMode(MOTOR_POWER_PIN, INPUT);
    pinMode(SPEED_PIN, INPUT);
    timer0_setup();
    pinMode(DIRECTION_LEFT_PIN, INPUT);
    pinMode(DIRECTION_RIGHT_PIN, INPUT);
}

void loop()
{
  unsigned char cur_t0, last_t0 = 0;
  int sub_sec_count = 0;    // 
  
  char last_mot = -1, cur_mot, pend_mot = 0;
  char last_pwr = -1, cur_pwr, pend_pwr = 0;
  char last_spd = -1, cur_spd, pulse_active = 0;
  char last_dir, cur_dir = -1;

  Serial.write( '$' );

do {
  /* check our clock wall time */  
    {
      cur_t0 = TCNT0;
      {
        char ticks = cur_t0 - last_t0;  
        sub_sec_count += ticks;
      }
      last_t0 = cur_t0;
      
      if (sub_sec_count >= DEBOUNCE_TICKS) {
        char any_change = 0;

        sub_sec_count -= DEBOUNCE_TICKS;

        cur_mot = digitalRead(MOTOR_SELECT_PIN);
        if (last_mot != cur_mot) {
           if (pend_mot) {
            if (pulse_active) {
                Serial.write('0');              
                pulse_active = 0;
            }
            if (MOTOR_POWER_ON == cur_pwr) {
                Serial.write( (last_mot == MOTOR_SELECT_A) ? 'a' : 'b' );
            }
          any_change = 1;
          pend_mot = 0;
          last_mot = cur_mot;
        }  else
          pend_mot = 1;
        }
  
        cur_pwr = digitalRead(MOTOR_POWER_PIN);
        if (last_pwr != cur_pwr) {
          if (pend_pwr) {
            any_change = 1;
            pend_pwr = 0;
            last_pwr = cur_pwr;
          } else
             pend_pwr = 1;
        }
        
        if (any_change) {
            if (MOTOR_POWER_ON == cur_pwr) {
                Serial.write( (cur_mot == MOTOR_SELECT_A) ? 'A' : 'B' );
            } else {              
                Serial.write( (cur_mot == MOTOR_SELECT_A) ? 'a' : 'b' );
            }
        }
  
        cur_spd = digitalRead(SPEED_PIN);
    	if (last_spd != cur_spd) {
    	  if (pulse_active) {
          Serial.write( (cur_spd == SPEED_FAST) ? SPEED_FAST_CHAR : SPEED_SLOW_CHAR );
    	  }
    	  last_spd = cur_spd;
    	}

      cur_dir = 0;
      if (DIRECTION_PRESSED == digitalRead(DIRECTION_LEFT_PIN)) {
        if (DIRECTION_PRESSED == digitalRead(DIRECTION_RIGHT_PIN)) {
          Serial.write('\\'); 
        } else
          cur_dir = 1;
      } else if (DIRECTION_PRESSED == digitalRead(DIRECTION_RIGHT_PIN)) {
          cur_dir = 2;
      }
     if (last_dir != cur_dir) {
      if (cur_dir) {
          Serial.write( (cur_dir == 1) ? 'L' : 'R' );
          Serial.write( (cur_spd == SPEED_FAST) ? SPEED_FAST_CHAR : SPEED_SLOW_CHAR );
      	  pulse_active = 1;
            } else {
                Serial.write('0');
      	  pulse_active = 0;
      	}
        last_dir = cur_dir;
     }
        }
    }
      if (Serial.available() > 0)
      {
       char incomingByte = Serial.read();
        if (incomingByte != '.')
          Serial.write( incomingByte );
      }
    } while(1);
}

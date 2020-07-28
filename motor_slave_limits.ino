 /* remote antenna tuner, wireless motor test based on
  * track pacer node
  (c) 2020 Lyle Hazel, Len Long, Howard Mintz 
  see LICENSE.txt */

#define     MAX_MOTORS_SUPPORTED  2
#define     NUM_SPEEDS_IN_TABLE   8

#define     NUM_POWER_PINS       1

#define     NUM_SECONDS_FAULT     7

enum _motor_pins {
  MOTOR_PULSE,
  MOTOR_DIRECTION,
  MOTOR_LIMIT_LEFT,
  MOTOR_LIMIT_RIGHT,
  NUM_PINS_PER_MOTOR
};

const char pin_table [MAX_MOTORS_SUPPORTED][NUM_PINS_PER_MOTOR] = {
  {  6,  5,  2,  3 },   // motor A
  {  8,  9, 11, 12 },	// motor B
};

#if NUM_POWER_PINS == 1
# define     MOTOR_POWER_PIN    4
#else
 const char power_pins[NUM_POWER_PINS] = {
   4, 10  };
#endif

#define     MOTOR_POWER_ON     LOW
#define     MOTOR_POWER_OFF   HIGH

#define     DIRECTION_LEFT     LOW
#define     DIRECTION_RIGHT   HIGH

#define     PULSE_IDLE       LOW
#define     PULSE_ACTIVE     HIGH

#define     LIMIT_ACTIVE      LOW
#define     LIMIT_NOT_ACTIVE  HIGH


 /* Set Timer0 for 15,625 KHz -> 16MHz / 2^10 = 5^6
   * we must read Timer0 faster than 256/15k = 16 msec */

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

const int clock_rates[NUM_SPEEDS_IN_TABLE] = {
    T0_COUNTS_PER_SECOND /   3,
    T0_COUNTS_PER_SECOND /   6,
    T0_COUNTS_PER_SECOND /  12,
    T0_COUNTS_PER_SECOND /  25,
    T0_COUNTS_PER_SECOND /  50,
    T0_COUNTS_PER_SECOND / 100,
    T0_COUNTS_PER_SECOND / 200,
    T0_COUNTS_PER_SECOND / 400
};

#define     MAX_PULSE_TICKS       (T0_COUNTS_PER_SECOND /  5)
#define     MIN_PULSE_TICKS       (T0_COUNTS_PER_SECOND / 800)

#define     NUM_PINS_QUERY     14
char pin_is_reserved[NUM_PINS_QUERY] = {
	1, 1, 0, 0,	0, 0, 0, 0,
	0, 0, 0, 0,	0, 0		};

void setup()
{
  char pin;
  int i = 0;
  Serial.begin(9600);
  do  {
    pin = pin_table[i][MOTOR_PULSE];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, PULSE_IDLE);
    pin_is_reserved[pin] = 1;
    pin = pin_table[i][MOTOR_DIRECTION];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, DIRECTION_RIGHT);
    pin_is_reserved[pin] = 1;

    pin = pin_table[i][MOTOR_LIMIT_LEFT];
    pinMode(pin, INPUT);
    pin = pin_table[i][MOTOR_LIMIT_RIGHT];
    pinMode(pin, INPUT);

    i ++;
  } while (i < MAX_MOTORS_SUPPORTED);
  
#if NUM_POWER_PINS == 1
    pin = MOTOR_POWER_PIN;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, MOTOR_POWER_OFF);
    pin_is_reserved[pin] = 1;
#else
  i = 0;
  do {
    pin = power_pins[i];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, MOTOR_POWER_OFF);
    pin_is_reserved[pin] = 1;
    i ++;
  } while (i < NUM_POWER_PINS);
#endif

  timer0_setup();
}

const char sign_on1[] = "W1ZTL stepper slave for \0";
const char sign_on2[] = " motors, with limits.\n\0";

const char help1[] = "Send A, B to turn on motors, a, b to turn off.\n\0";
const char help2[] = "Send L, R for dir, 1 - 9 to step.\n\0";

void print_str(const char * str)
{
  char ch;
  int i = 0;
  do {
    ch = str[i];
    Serial.write(ch);
    i ++;
  } while (ch);
}

void hello_world(void)
{
  print_str(sign_on1);
  Serial.write('0' + MAX_MOTORS_SUPPORTED);
  print_str(sign_on2);
}

char cur_powered[MAX_MOTORS_SUPPORTED] =
  {  -1, -1,  };

char pulse_active = 0;

void power_setting(int mot, int on)
{
    if (on == cur_powered[mot])
        Serial.write('*');
    else {
        if (mot < NUM_POWER_PINS)
	{
	  char pin;
#if NUM_POWER_PINS == 1
	  pin = MOTOR_POWER_PIN;
#else
	  pin = power_pins[mot];
#endif
	  digitalWrite(pin, (on) ? MOTOR_POWER_ON : MOTOR_POWER_OFF);
      }
      cur_powered[mot] = on;
      Serial.write(((on) ? 'A' : 'a') + mot);
    }
}

char g_cur_mot = -1;
char g_cur_dir = -1;
char g_hit_limit = 0;

void set_pulse(int on)
{
  char pulsePin = pin_table[g_cur_mot][MOTOR_PULSE];
  if (on)
  {
    char   limPin = pin_table[g_cur_mot][(g_cur_dir == DIRECTION_LEFT) ? MOTOR_LIMIT_LEFT : MOTOR_LIMIT_RIGHT];
    pulse_active = 1;
    if (LIMIT_ACTIVE != digitalRead(limPin)) {
      digitalWrite(pulsePin, PULSE_ACTIVE);
      if (g_hit_limit) {
        Serial.write('_');
        g_hit_limit = 0;
      }
    } else {
      if (0 == g_hit_limit) {
        Serial.write('!');
        g_hit_limit = 1;
      }
    }
  }
  else {
    digitalWrite(pulsePin, PULSE_IDLE);
    pulse_active = 0;
  }
}

void loop()
{
  unsigned char cur_t0, last_t0 = 0;
  int pulse_counter;    // number of 64 usec units since last pulse
  int sub_sec_count = 0;    // number of 64 usec units since the last ping
  unsigned char seconds;
  
  char cur_speed = 0;

  hello_world();

do {
  /* check our clock wall time */  
    {
      char ticks;
      cur_t0 = TCNT0;
      ticks = cur_t0 - last_t0;
      last_t0 = cur_t0;

    if (g_cur_mot != -1) {

    if (cur_speed) {
      int cur_rate = MAX_PULSE_TICKS * 2;

      pulse_counter += ticks;
      sub_sec_count += ticks;
     if (cur_speed > 1)
         cur_rate = clock_rates[cur_speed - 2];

     if ((pulse_active) &&
         (pulse_counter >= (cur_rate / 2))) {
       set_pulse(0);
       if (cur_speed == 1) {
        cur_speed = 0;
        Serial.write('\\');
       }
     }
       else if (pulse_counter >= cur_rate) {
	 set_pulse(1);
         pulse_counter -= cur_rate;
       }
      if (sub_sec_count >= T0_COUNTS_PER_SECOND) {
        sub_sec_count -= T0_COUNTS_PER_SECOND;
        seconds ++;
        if (seconds >= NUM_SECONDS_FAULT) {
          set_pulse(0);
          cur_speed = 0;
          Serial.write('#');
        } else
          Serial.write('.');
      }
      }
     }
    }

  if (Serial.available() > 0) 
  {
  char incomingByte = Serial.read();
  if ((incomingByte >= 'A') &&
      (incomingByte < 'A' + MAX_MOTORS_SUPPORTED))  {
    // power up motor controller 
    g_cur_mot = incomingByte - 'A';
     power_setting( g_cur_mot, 1);
  } else
  if ((incomingByte >= 'a') &&
      (incomingByte < 'a' + MAX_MOTORS_SUPPORTED))  {
    // power down motor controller 
    g_cur_mot = incomingByte - 'a';
     power_setting( g_cur_mot, 0);
  } else
  if (incomingByte == 'L') {
    g_cur_dir = DIRECTION_LEFT;
    goto set_dir;
  } else if (incomingByte == 'R') {
    char pin;
    g_cur_dir = DIRECTION_RIGHT;
  set_dir:
    if ((g_cur_mot >= 0) &&
        (g_cur_mot < MAX_MOTORS_SUPPORTED)) {
      pin = pin_table[g_cur_mot][MOTOR_DIRECTION];
      digitalWrite(pin, g_cur_dir);
      Serial.write(incomingByte);
    } else
     Serial.write('&');
  }
  else if (incomingByte == 's') {
    int i = 0;
    do  {
      Serial.write(((cur_powered[i]) ? 'A' : 'a') + i);
      if (i == g_cur_mot) {
        if (g_cur_dir == DIRECTION_LEFT)
          Serial.write('L');
        else if (g_cur_dir == DIRECTION_RIGHT)
          Serial.write('R');
      }
      i ++;
      if (i >= MAX_MOTORS_SUPPORTED)
        break;
      Serial.write(',');
    } while (1);
  }
  else  if ((incomingByte == 'I') ||
            (incomingByte == 'i')) {
    int i = 0;
    do	{
	if (0 == pin_is_reserved[i])
	{
	    char ones, value = digitalRead(i);
	    ones = (i % 10) + '0';
	    Serial.write('p');
	    if (10 <= i)
	    	Serial.write('1');
	    Serial.write(ones);
	    Serial.write(':');
	    Serial.write((value) ? 'H' : 'L');
	    Serial.write(',');
	}
	i ++;
	} while (i < NUM_PINS_QUERY);
  }
  else  if ((incomingByte == 'H') ||
            (incomingByte == 'h')) {
    print_str(help1);
    print_str(help2);
  }
  else  if (incomingByte == 'S') {
    int i = 0;
    do  {
      if (cur_powered[i])
          power_setting(i, 0);
      i ++;
    } while (i < MAX_MOTORS_SUPPORTED);
    Serial.write('-');
  }
   else if ((incomingByte >= '0') && (incomingByte <= ('0' + NUM_SPEEDS_IN_TABLE + 1))) {
    char new_speed  = incomingByte - '0';
    if (g_cur_mot != -1) {
      if (cur_speed != new_speed) {
       pulse_counter =    // number of 1.6 millisec units since last pulse
       sub_sec_count = 0; // number of 1.6 millisec units since the last ping

       set_pulse( (new_speed) ? 1 : 0);

       if (new_speed == 1)
         Serial.write('/');
       else if (new_speed)
         Serial.write('<');
      else if (cur_speed)
         Serial.write('>');
       cur_speed = new_speed;
      }
     seconds = 0;
    }
    else
     Serial.write('@');
   } else
   if ((incomingByte == ' ') ||
       (incomingByte == '\n') ||
       (incomingByte == '\r'))
      Serial.write(incomingByte);
    else
      Serial.write('?');
                }
      
    } while(1);
}

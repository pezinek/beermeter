#include <LiquidCrystal.h>
#include <LCDKeypad.h>
#include <EEPROM.h>

#define FLOW_GAUGE_PIN 2

#define DFLT_PULSES_PER_LITRE 4380

#define BEER_CHAR 1
byte c_beer[8] = {
  B01110,
  B10001,
  B10001,
  B11111,
  B11111,
  B11111,
  B11111,
  B01110,
};

#define BEER_HANDLE_CHAR 2
byte c_beer_handle[8] = {
  B00000,
  B00000,
  B11000,
  B00100,
  B00100,
  B11000,
  B00000,
  B00000,
};

#define SHOT_CHAR 3
byte c_shot[8] = {
  B00000,
  B00000,
  B01100,
  B10010,
  B10010,
  B11110,  
  B11110,  
  B01100,  
};

#define DROP_CHAR 4
byte c_drop[8] = {
  B00000,
  B00100,
  B00100,
  B01010,
  B01010,
  B10001,
  B10001,
  B01110,  
};

#define PULSE_CHAR 5
byte c_pulse[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B01110,
  B01010,
  B11011,
  B00000,
};

static const int keg_sizes[] = {5,10,15,20,30,50};
int keg_selected = 1;

LCDKeypad lcd;
volatile unsigned long int pulses=0;

struct Eprom_Configuration {
  char signature;
  int pulses_per_liter;
};

#define EEPROM_CFG_START  (256 - sizeof(Eprom_Configuration))
#define EEPROM_SIGNATURE  0xBE

Eprom_Configuration cfg = {
  EEPROM_SIGNATURE,        //signature
  DFLT_PULSES_PER_LITRE
};

struct displayMode {
  char left;
  char right;
  char up;
  char down;
  char sel;
  void (*display_callback)();
  void (*activate_callback)();
};

char active_mode=1;
char previous_mode=1;
static const displayMode modes[] = {
  //Left, Right, Up, Down, Select, Display_Fn, Activate_Fn
  {0xFF, 0xFF, 0x04, 0x01, 0xFF, display_raw, noop}, // O
  {0x02, 0x03, 0x00, 0x04, 0xFF, display_keg, noop}, // 1
  {0x02, 0x03, 0x00, 0x04, 0xFF, display_keg, dec_keg}, // 2
  {0x02, 0x03, 0x00, 0x04, 0xFF, display_keg, inc_keg}, // 3
  {0x05, 0x06, 0x01, 0x00, 0xFF, display_calibration, noop}, // 4
  {0x05, 0x06, 0x01, 0x00, 0x07, display_calibration, dec_calibration}, // 5
  {0x05, 0x06, 0x01, 0x00, 0x07, display_calibration, inc_calibration}, // 6
  {0x05, 0x06, 0x01, 0x00, 0xFF, display_saved_msg, save_cfg} // 7  
};

void set_mode(char mode) {
  if (mode > (sizeof(modes) / sizeof(*modes))) return;
  previous_mode=active_mode;
  active_mode=mode;
  modes[active_mode].activate_callback();
}

void noop() {
  return;
}

void display_volume(float liters) 
{
  int beers = liters * 2;
  liters = liters - ((float) beers / 2);
  int shots = liters * 50;
  liters = liters - ((float) shots / 50);
  int drops = liters * 30000;
  
  lcd.print(beers);
  lcd.write(' ');
  lcd.write(BEER_CHAR);
  lcd.write(BEER_HANDLE_CHAR);
  lcd.write(' ');
  lcd.print(shots);
  lcd.write(' ');
  lcd.write(SHOT_CHAR);
  lcd.write(' ');
  lcd.print(drops);
  lcd.write(' ');
  lcd.write(DROP_CHAR);
  lcd.write(' ');
}

void display_raw()
{
  float liters;
  liters = (float) pulses / cfg.pulses_per_liter;
    
  lcd.clear();
  lcd.print("VYPITO: ");
  lcd.print(pulses);
  lcd.write(PULSE_CHAR);
  lcd.setCursor(0,1);
  display_volume(liters);
}

void inc_keg() 
{
  keg_selected++;
  if (keg_selected >= sizeof(keg_sizes)/sizeof(int)) {
    keg_selected=0;
  }
}

void dec_keg()
{
  keg_selected--;
  if (keg_selected < 0) {
    keg_selected=(sizeof(keg_sizes)/sizeof(int))-1;
  }
}

void display_keg()
{
  int max_vol = keg_sizes[keg_selected];
  
  lcd.clear();
  lcd.print("ZBYVA: <SUD ");
  lcd.print(max_vol);
  lcd.print("L>");
  lcd.setCursor(0,1);
  
  float liters_left = (float) max_vol - (float) pulses / cfg.pulses_per_liter;  
  display_volume(liters_left);
}

void inc_calibration()
{
  cfg.pulses_per_liter++;
}

void dec_calibration()
{
  cfg.pulses_per_liter--;
}

void display_calibration()
{
  lcd.clear();
  lcd.print("<  KALIBRACE   >");
  lcd.setCursor(0,1);
  lcd.print(cfg.pulses_per_liter);
  lcd.write(' ');
  lcd.write(PULSE_CHAR);
  lcd.print("/L");
}

void display_saved_msg()
{
  lcd.clear();
  lcd.print("<  KALIBRACE   >");
  lcd.setCursor(0,1);
  lcd.print("    ULOZENO     ");
}

void pulse()
{
  pulses++;
}

void load_cfg() {
  if (EEPROM.read(EEPROM_CFG_START) == EEPROM_SIGNATURE) {
    for (unsigned int t=0; t<sizeof(cfg); t++) {
      *((char *)&cfg + t) = EEPROM.read(EEPROM_CFG_START + t);      
    }
  }
}

void save_cfg() {
  for (unsigned int t=0; t<sizeof(cfg); t++) {
    EEPROM.write(EEPROM_CFG_START+t, *((char*)&cfg + t));
  }  
}

void setup()
{
  Serial.begin(9600);
  Serial.println("[Pivomerka]");
  lcd.createChar(BEER_CHAR, c_beer);
  lcd.createChar(BEER_HANDLE_CHAR, c_beer_handle);
  lcd.createChar(SHOT_CHAR, c_shot);
  lcd.createChar(DROP_CHAR, c_drop);
  lcd.createChar(PULSE_CHAR, c_pulse);
  lcd.begin(16,2);
  lcd.clear();
  lcd.print(" [PIVOMERKA] ");
  pinMode(FLOW_GAUGE_PIN, INPUT);
  load_cfg();
  attachInterrupt(1, pulse, RISING);
}

static unsigned long btn_click_len = 500L;
unsigned long last_btn_click = 0L;
static unsigned long display_period = 500L;
unsigned long int last_display_time = 0L;

void loop()
{
  unsigned long int now = millis();
  
  if (now - last_btn_click > btn_click_len) {
    last_btn_click=now;
    switch(lcd.button()) {
      case KEYPAD_UP: set_mode(modes[active_mode].up); break;
      case KEYPAD_DOWN: set_mode(modes[active_mode].down); break;
      case KEYPAD_LEFT: set_mode(modes[active_mode].left); break;
      case KEYPAD_RIGHT: set_mode(modes[active_mode].right); break;
      case KEYPAD_SELECT: set_mode(modes[active_mode].sel); break;  
      case KEYPAD_NONE: last_btn_click = 0; 
    }
  }  
  
  if (now - last_display_time > display_period) {
    last_display_time = now;
    modes[active_mode].display_callback();
  }
}

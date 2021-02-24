#include "PS2KeyAdvanced.h"
#include "matrix.h"
#include <EEPROM.h>
#include <SPI.h>


#define DEBUG_MODE 0
#define DEBUG_KEY 0

// ---- Pins for Atmega328
#define PIN_KBD_CLK 3 // PD3
#define PIN_KBD_DAT 2 // PD2

// hardware SPI
#define PIN_SS 10 // SPI slave select

//Leds on PCB
#define LED_PWR A0
#define LED_KBD A1
#define LED_TURBO A2
#define LED_SPECIAL A3

#define EEPROM_TURBO_ADDRESS 0x00
#define EEPROM_SPECIAL_ADDRESS 0x01

#define EEPROM_VALUE_TRUE 10
#define EEPROM_VALUE_FALSE 20

PS2KeyAdvanced kbd;

bool matrix[ZX_MATRIX_FULL_SIZE]; // matrix of pressed keys + special keys to be transmitted on CPLD side by SPI protocol

bool blink_state = false;

bool is_turbo = false;
bool is_special = false;
bool is_caps = false;

unsigned long t = 0;  // current time
unsigned long tl = 0; // blink poll time
unsigned long te = 0; // eeprom store time

// keyboard leds bits
uint8_t leds = 0b00000000;

SPISettings settingsA(8000000, MSBFIRST, SPI_MODE0); // SPI transmission settings

// transform PS/2 scancodes into internal matrix of pressed keys
void fill_kbd_matrix(int sc)
{

  static bool is_up=false;
  static bool is_ctrl=false, is_alt=false, is_del=false, is_bksp = false, is_shift = false, is_esc = false, is_ss_used = false, is_cs_used = false, is_caps_used = false;

  //BREAK/PAUSE key check not in switch-case ScanCode: 0x6
  //Special key as 1024/128 - BREAK/PAUSE
  if ( sc == 0b110 ) {
    is_special = !is_special;
    eeprom_store_value(EEPROM_SPECIAL_ADDRESS, is_special);
    matrix[ZX_K_SPECIAL] = is_special;
    bitWrite(leds, 1, is_special);
    kbd.setLock(leds);
  #if DEBUG_KEY
    Serial.print(F("128/1024 SPECIAL (PAUSE/BRAKE): "));
    Serial.println(sc, HEX);
    Serial.println(leds, BIN);
  #endif
    //return;
  }

  //check num lock button -> 0x101(ON), 0x8101(OFF); when CAPS LOCK is ON: 0x1101(ON), 0x9101(OFF)
  if ( sc == 0x101 || sc == 0x8101 || sc == 0x1101 || sc == 0x9101 ) {
    kbd.setLock(leds);
    #if DEBUG_KEY
      Serial.print(F("NUM LOCK IN BLOCK! "));
      Serial.print(F("Leds: "));
      Serial.println(leds, BIN);
    #endif
    return;
  }

  //check CAPS LOCK
  if (is_caps) {
    matrix[ZX_K_CS] = true;
  }

  // is key released prefix
  if (sc & PS2_BREAK && !is_up) {
    is_up = 1;
    #if DEBUG_KEY
      Serial.println("BREAK!");
    #endif
    //return;
  }

  uint8_t scancode = sc & 0xFF;

  is_ss_used = false;
  is_cs_used = false;
  is_caps_used = false;

  switch (scancode) {

    // CapsLock
    case PS2_KEY_CAPS:
      //matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_CS] = !is_up;
      //is_cs_used = !is_up;
      is_caps = !is_up;
      bitWrite(leds, 2, !is_up);
      kbd.setLock(leds);
      #if DEBUG_KEY
        Serial.print(F("CS (CAPS): "));
        Serial.println(matrix[ZX_K_CS]);
        Serial.println(scancode, HEX);
        Serial.print(F("leds: "));
        Serial.println(leds, BIN);
      #endif
      break;
  
    // Shift -> CS for ZX
    case PS2_KEY_L_SHIFT: 
    case PS2_KEY_R_SHIFT:
      if (sc != 0b110) {  //Check if not BRAKE/PAUSE button
        matrix[ZX_K_CS] = !is_up;
        is_shift = !is_up;
        #if DEBUG_KEY
          Serial.print(F("CS: "));
          Serial.println(scancode, HEX);
        #endif
      }
      break;

    // Ctrl -> SS for ZX
    case PS2_KEY_L_CTRL:
    case PS2_KEY_R_CTRL:
      matrix[ZX_K_SS] = !is_up;
      is_ctrl = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS: "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Alt (L) -> SS+CS for ZX
    case PS2_KEY_L_ALT:
      matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_CS] = !is_up;
      is_alt = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS+CS (ALT_L): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Alt (R) -> SS+CS for ZX
    case PS2_KEY_R_ALT:
      matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_CS] = !is_up;
      is_alt = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS+CS (ALT_R): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Del -> SS+C for ZX
    case PS2_KEY_DELETE:
      matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_C] =  !is_up;
      is_del = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS+C (DELETE): "));
        Serial.println(scancode, HEX);
      #endif
    break;

    // Ins -> SS+A for ZX
    case PS2_KEY_INSERT:
       matrix[ZX_K_SS] = !is_up;
       matrix[ZX_K_A] =  !is_up;
       #if DEBUG_KEY
        Serial.print(F("SS+A (INSERT): "));
        Serial.println(scancode, HEX);
       #endif
    break;

    // Cursor -> CS + 5,6,7,8
    case PS2_KEY_UP_ARROW:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_7] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("UP: "));
        Serial.println(scancode, HEX);
      #endif
      break;
      
    case PS2_KEY_DN_ARROW:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_6] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("DOWN: "));
        Serial.println(scancode, HEX);
      #endif
      break;
      
    case PS2_KEY_L_ARROW:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_5] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("LEFT: "));
        Serial.println(scancode, HEX);
      #endif
      break;
      
    case PS2_KEY_R_ARROW:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_8] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("RIGHT: "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // ESC -> CS+SPACE for ZX
    case PS2_KEY_ESC:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_SP] = !is_up;
      is_cs_used = !is_up;
      is_esc = !is_up;
      #if DEBUG_KEY
        Serial.print(F("CS+SPACE (ESC): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Backspace -> CS+0
    case PS2_KEY_BS:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_0] = !is_up;
      is_cs_used = !is_up;
      is_bksp = !is_up;
      #if DEBUG_KEY
        Serial.print(F("CS+0 (BACKSPACE): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Enter
    case PS2_KEY_ENTER:
    case PS2_KEY_KP_ENTER:
      matrix[ZX_K_ENT] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("ENTER: "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Space
    case PS2_KEY_SPACE:
      matrix[ZX_K_SP] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SPACE: "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Letters & numbers
    case PS2_KEY_A: matrix[ZX_K_A] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("A: ")); Serial.println(scancode, HEX);
      #endif 
      break;
    case PS2_KEY_B: matrix[ZX_K_B] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("B: ")); Serial.println(scancode, HEX);
      #endif 
      break;
    case PS2_KEY_C: matrix[ZX_K_C] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("C: ")); Serial.println(scancode, HEX); 
      #endif
      break;
    case PS2_KEY_D: matrix[ZX_K_D] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("D: ")); Serial.println(scancode, HEX); 
      #endif
      break;
    case PS2_KEY_E: matrix[ZX_K_E] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("E: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_F: matrix[ZX_K_F] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("F: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_G: matrix[ZX_K_G] = !is_up; 
      #if DEBUG_KEY
        Serial.print(F("G: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_H: matrix[ZX_K_H] = !is_up; 
      #if DEBUG_KEY
        Serial.print(F("H: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_I: matrix[ZX_K_I] = !is_up; 
      #if DEBUG_KEY 
        Serial.print(F("I: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_J: matrix[ZX_K_J] = !is_up;
      #if DEBUG_KEY 
        Serial.print(F("J: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_K: matrix[ZX_K_K] = !is_up; 
      #if DEBUG_KEY
        Serial.print(F("K: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_L: matrix[ZX_K_L] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("L: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_M: matrix[ZX_K_M] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("M: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_N: matrix[ZX_K_N] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("N: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_O: matrix[ZX_K_O] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("O: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_P: matrix[ZX_K_P] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("P: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_Q: matrix[ZX_K_Q] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("Q: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_R: matrix[ZX_K_R] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("R: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_S: matrix[ZX_K_S] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("S: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_T: matrix[ZX_K_T] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("T: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_U: matrix[ZX_K_U] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("U: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_V: matrix[ZX_K_V] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("V: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_W: matrix[ZX_K_W] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("W: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_X: matrix[ZX_K_X] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("X: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_Y: matrix[ZX_K_Y] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("Y: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_Z: matrix[ZX_K_Z] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("Z: ")); Serial.println(scancode, HEX);
      #endif
      break;

    // digits
    case PS2_KEY_0: matrix[ZX_K_0] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("0: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_1: matrix[ZX_K_1] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("1: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_2: matrix[ZX_K_2] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("2: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_3: matrix[ZX_K_3] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("3: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_4: matrix[ZX_K_4] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("4: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_5: matrix[ZX_K_5] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("5: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_6: matrix[ZX_K_6] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("6: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_7: matrix[ZX_K_7] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("7: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_8: matrix[ZX_K_8] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("8: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_9: matrix[ZX_K_9] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("9: ")); Serial.println(scancode, HEX);
      #endif
      break;

    // Keypad digits
    case PS2_KEY_KP0: matrix[ZX_K_0] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 0: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP1: matrix[ZX_K_1] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 1: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP2: matrix[ZX_K_2] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 2: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP3: matrix[ZX_K_3] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 3: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP4: matrix[ZX_K_4] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 4: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP5: matrix[ZX_K_5] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 5: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP6: matrix[ZX_K_6] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 6: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP7: matrix[ZX_K_7] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 7: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP8: matrix[ZX_K_8] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 8: ")); Serial.println(scancode, HEX);
      #endif
      break;
    case PS2_KEY_KP9: matrix[ZX_K_9] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("KP 9: ")); Serial.println(scancode, HEX);
      #endif
      break;

    // '/" -> SS+P / SS+7
    case PS2_KEY_APOS:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_P : ZX_K_7] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+P/SS+7 (QUOTE): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // ,/< -> SS+N / SS+R
    case PS2_KEY_COMMA:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_R : ZX_K_N] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+N/SS+R (COMMA): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // ./> -> SS+M / SS+T
    case PS2_KEY_DOT:
    case PS2_KEY_KP_DOT:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_T : ZX_K_M] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+M/SS+T (PERIOD): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // ;/: -> SS+O / SS+Z
    case PS2_KEY_SEMI:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_Z : ZX_K_O] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+0/SS+Z (SEMICOLON): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // [,{ -> SS+Y / SS+F
    case PS2_KEY_OPEN_SQ:
      if (!is_up) {
        send_macros( (is_shift or is_caps) ? ZX_K_F : ZX_K_Y);
      }
      #if DEBUG_KEY
        Serial.print(F("SS+Y/SS+F (L BRACKET): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // ],} -> SS+U / SS+G
    case PS2_KEY_CLOSE_SQ:
      if (!is_up) {
        send_macros( (is_shift or is_caps) ? ZX_K_G : ZX_K_U);
      }
      #if DEBUG_KEY
        Serial.print(F("SS+U/SS+G (R BRACKET): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // /,? -> SS+V / SS+C
    case PS2_KEY_DIV:
    case PS2_KEY_KP_DIV:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_C : ZX_K_V] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+V/SS+C (SLASH): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // \,| -> SS+D / SS+S
    case PS2_KEY_BACK:
    case PS2_KEY_EUROPE2:
      if (!is_up) {
        send_macros( (is_shift or is_caps) ? ZX_K_S : ZX_K_D);
      }
      #if DEBUG_KEY
        Serial.print(F("SS+D/SS+S (BACK SLASH): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // =,+ -> SS+L / SS+K
    case PS2_KEY_EQUAL:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_K : ZX_K_L] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+L/SS+K (EQUALS): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // -,_ -> SS+J / SS+0
    case PS2_KEY_MINUS:
      matrix[ZX_K_SS] = !is_up;
      matrix[ (is_shift or is_caps) ? ZX_K_0 : ZX_K_J] = !is_up;
      is_caps_used = is_caps ? true : false;
      is_ss_used = is_shift;
      #if DEBUG_KEY
        Serial.print(F("SS+J/SS+0 (MINUS/_): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // `,~ -> SS+X / SS+A
    case PS2_KEY_SINGLE:
      if ( (is_shift or is_caps) and !is_up) {
        send_macros( (is_shift or is_caps) ? ZX_K_A : ZX_K_X);
      }
      if (!is_shift or !is_caps) {
        matrix[ZX_K_SS] = !is_up;
        matrix[ZX_K_X] = !is_up;
        is_ss_used = is_shift;
      }
      #if DEBUG_KEY
        Serial.print(F("SS+X/SS+A (ACCENT): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Keypad * -> SS+B
    case PS2_KEY_KP_TIMES:
      matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_B] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS+B (*): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Keypad - -> SS+J
    case PS2_KEY_KP_MINUS:
      matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_J] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS+J (-): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Keypad + -> SS+K
    case PS2_KEY_KP_PLUS:
      matrix[ZX_K_SS] = !is_up;
      matrix[ZX_K_K] = !is_up;
      #if DEBUG_KEY
        Serial.print(F("SS+K (+): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Tab
    case PS2_KEY_TAB:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_I] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("CS+I (TAB): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // PgUp -> CS+3 for ZX
    case PS2_KEY_PGUP:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_3] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("CS+3 (PGUP): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // PgDn -> CS+4 for ZX
    case PS2_KEY_PGDN:
      matrix[ZX_K_CS] = !is_up;
      matrix[ZX_K_4] = !is_up;
      is_cs_used = !is_up;
      #if DEBUG_KEY
        Serial.print(F("CS+4 (PGDN): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    // Scroll Lock -> Turbo
    case PS2_KEY_SCROLL:
      is_turbo = !is_turbo;
      eeprom_store_value(EEPROM_TURBO_ADDRESS, is_turbo);
      matrix[ZX_K_TURBO] = !is_turbo;
      bitWrite(leds, 0, is_turbo);
      kbd.setLock(leds);
      #if DEBUG_KEY
        Serial.print(F("TURBO (SCROLL LOCK): "));
        Serial.println(sc, HEX);
        Serial.println(leds, BIN);
      #endif
      break;

    // PrintScreen -> Magick button
    case PS2_KEY_PRTSCR: 
      if (is_up) {
        do_magick();
      }
      #if DEBUG_KEY
        Serial.print(F("MAGICK (PSCR1): "));
        Serial.println(scancode, HEX);
      #endif
      break;

    case PS2_KEY_F12:
      if (is_up) {  // if key is released!
        is_ctrl = false;
        is_alt = false;
        is_del = false;
        is_shift = false;
        is_ss_used = false;
        is_cs_used = false;
        is_caps_used = false;
        do_reset();
        #if DEBUG_KEY
          Serial.print(F("RESET (F12): "));
          Serial.println(scancode, HEX);
        #endif
      }
      break;
  
  }

  if (is_ss_used and !is_cs_used) {
      matrix[ZX_K_CS] = false;
  }

  if (is_caps_used and !is_up) {
      matrix[ZX_K_CS] = false;  
  }

  // Ctrl+Alt+Del -> RESET
  if (is_ctrl && is_alt && is_del) {
    is_ctrl = false;
    is_alt = false;
    is_del = false;
    is_shift = false;
    is_ss_used = false;
    is_cs_used = false;
    is_caps_used = false;
    do_reset();
    #if DEBUG_KEY
        Serial.print(F("H RESET (ALT+CTRL+DEL): "));
        Serial.println(scancode, HEX);
    #endif
  }

  // Ctrl+Alt+Bksp -> REINIT controller
  if (is_ctrl && is_alt && is_bksp) {
    is_ctrl = false;
    is_alt = false;
    is_bksp = false;
    is_shift = false;
    is_ss_used = false;
    is_cs_used = false;
    is_caps_used = false;
    clear_matrix(ZX_MATRIX_SIZE);
    matrix[ZX_K_RESET] = true;
    transmit_keyboard_matrix();
    //matrix[ZX_K_S] = true;
    //transmit_keyboard_matrix();
    delay(500);
    matrix[ZX_K_RESET] = false;
    transmit_keyboard_matrix();
    //delay(500);
    //matrix[ZX_K_S] = false;
    #if DEBUG_KEY
      Serial.print(F("REINIT CONTROLLER (CTR+ALT+BACKSPACE): "));
      Serial.println(scancode, HEX);
    #endif
  }

   // clear flags
   is_up = 0;
}


uint8_t get_matrix_byte(uint8_t pos)
{
  uint8_t result = 0;
  for (uint8_t i=0; i<8; i++) {
    uint8_t k = pos*8 + i;
    if (k < ZX_MATRIX_FULL_SIZE) {
      bitWrite(result, i, matrix[k]);
    }
  }
  return result;
}

void spi_send(uint8_t addr, uint8_t data) 
{
  SPI.beginTransaction(settingsA);
  digitalWrite(PIN_SS, LOW);
  uint8_t cmd = SPI.transfer(addr); // command (1...6)
  uint8_t res = SPI.transfer(data); // data byte
  digitalWrite(PIN_SS, HIGH);
  SPI.endTransaction();
}

// transmit keyboard matrix from AVR to CPLD side via SPI
void transmit_keyboard_matrix()
{
  uint8_t bytes = 6;
  for (uint8_t i=0; i<bytes; i++) {
    uint8_t data = get_matrix_byte(i);
    spi_send(i+1, data);
  }
}

// transmit keyboard macros (sequence of keyboard clicks) to emulate typing some special symbols [, ], {, }, ~, |, `
void send_macros(uint8_t pos)
{
  clear_matrix(ZX_MATRIX_SIZE);
  transmit_keyboard_matrix();
  delay(20);
  matrix[ZX_K_CS] = true;
  transmit_keyboard_matrix();
  delay(20);
  matrix[ZX_K_SS] = true;
  transmit_keyboard_matrix();
  delay(20);
  matrix[ZX_K_SS] = false;
  transmit_keyboard_matrix();
  delay(20);
  matrix[pos] = true;
  transmit_keyboard_matrix();
  delay(20);
  matrix[ZX_K_CS] = false;
  matrix[pos] = false;
  transmit_keyboard_matrix();
  delay(20);
}

void do_reset()
{
  //clear_matrix(ZX_MATRIX_SIZE);
  if (is_caps) { matrix[ZX_K_CS] = false; }
  matrix[ZX_K_RESET] = true;
  transmit_keyboard_matrix();
  #if DEBUG_KEY
    Serial.println("RESET SEND!");
  #endif
  delay(200);
  matrix[ZX_K_RESET] = false;
  transmit_keyboard_matrix();
  delay(500);
  clear_matrix(ZX_MATRIX_SIZE);
  if (is_caps) {
    delay(100);
    matrix[ZX_K_CS] = false;
    transmit_keyboard_matrix();
  }
}

void do_magick()
{
  matrix[ZX_K_MAGICK] = true;
  transmit_keyboard_matrix();
  delay(500);
  matrix[ZX_K_MAGICK] = false;
  transmit_keyboard_matrix();
}

void clear_matrix(int clear_size)
{
  // all keys up
  for (int i=0; i<clear_size; i++) {
      matrix[i] = false;
  }
}

bool eeprom_restore_value(int addr, bool default_value)
{
  byte val;  
  val = EEPROM.read(addr);
  if ((val == EEPROM_VALUE_TRUE) || (val == EEPROM_VALUE_FALSE)) {
    return (val == EEPROM_VALUE_TRUE) ? true : false;
  } else {
    EEPROM.update(addr, (default_value ? EEPROM_VALUE_TRUE : EEPROM_VALUE_FALSE));
    return default_value;
  }
}

void eeprom_store_value(int addr, bool value)
{
  byte val = (value ? EEPROM_VALUE_TRUE : EEPROM_VALUE_FALSE);
  EEPROM.update(addr, val);
}

void eeprom_restore_values()
{
  is_turbo = eeprom_restore_value(EEPROM_TURBO_ADDRESS, is_turbo);
  is_special = eeprom_restore_value(EEPROM_SPECIAL_ADDRESS, is_special);
  bitWrite(leds, 0, is_turbo);
  bitWrite(leds, 1, is_special);
  kbd.setLock(leds);
  matrix[ZX_K_TURBO] = !is_turbo;
  matrix[ZX_K_SPECIAL] = is_special;
}

void eeprom_store_values()
{
  eeprom_store_value(EEPROM_TURBO_ADDRESS, is_turbo);
  eeprom_store_value(EEPROM_SPECIAL_ADDRESS, is_special);
  bitWrite(leds, 0, is_turbo);
  bitWrite(leds, 1, is_special);
  kbd.setLock(leds);
}

// initial setup
void setup()
{
  Serial.begin(115200);
  Serial.flush();
  SPI.begin();

  pinMode(PIN_SS, OUTPUT);
  digitalWrite(PIN_SS, HIGH);

  pinMode(LED_PWR, OUTPUT);
  pinMode(LED_KBD, OUTPUT);
  pinMode(LED_TURBO, OUTPUT);
  pinMode(LED_SPECIAL, OUTPUT);
  
  digitalWrite(LED_PWR, HIGH);
  digitalWrite(LED_KBD, HIGH);
  digitalWrite(LED_TURBO, LOW);
  digitalWrite(LED_SPECIAL, LOW);

  // ps/2
  pinMode(PIN_KBD_CLK, INPUT_PULLUP);
  pinMode(PIN_KBD_DAT, INPUT_PULLUP);

  // zx signals (output)

  // clear full matrix
  clear_matrix(ZX_MATRIX_FULL_SIZE);

  digitalWrite(LED_TURBO, is_turbo ? HIGH : LOW);
  digitalWrite(LED_SPECIAL, is_special ? HIGH: LOW);

  Serial.println(F("ZX PS/2 Keyboard controller v1.3b"));

  #if DEBUG_MODE
    Serial.println(F("done"));
    Serial.println(F("Keyboard init..."));
  #endif

  kbd.begin(PIN_KBD_DAT, PIN_KBD_CLK);
  kbd.resetKey();
  delay(100);
  kbd.echo( );// ping keyboard to see if there
  delay( 6 );
  
  // restore saved modes from EEPROM
  eeprom_restore_values();
  
  #if DEBUG_MODE
    Serial.println(F("Reset on boot..."));
  #endif

  do_reset();
  
#if DEBUG_MODEE
  uint16_t c = kbd.read( );
  if( (c & 0xFF) == PS2_KEY_ECHO || (c & 0xFF) == PS2_KEY_BAT ) {
    Serial.println( "Keyboard OK.." );    // Response was Echo or power up
    kbd.setLock( leds );
  }
  else
    if( ( c & 0xFF ) == 0 )
      Serial.println( "Keyboard Not Found" );
    else
      {
      Serial.print( "Invalid Code received of " );
      Serial.println( c, HEX );
      }
#endif

  digitalWrite(LED_KBD, LOW);  
}

// main loop
void loop()
{
  unsigned long n = millis();
  
  if (kbd.available()) {
    uint16_t c = kbd.read();
    if( ( c & 0xFF ) > 0 ) {
      blink_state = true;
      tl = n;
      digitalWrite(LED_KBD, HIGH);
      #if DEBUG_MODE    
          //Serial.print(F("Scancode: "));
          //Serial.println(c, HEX);
          Serial.print( "Value " );
          Serial.print( c, HEX );
          Serial.print( " - Status Bits " );
          Serial.print( c >> 8, HEX );
          Serial.print( "  Code " );
          Serial.println( c & 0xFF, HEX );
      #endif

      // if keyboard reconnect - update leds
      if( (c & 0xFF) == PS2_KEY_ECHO || (c & 0xFF) == PS2_KEY_BAT ) {
        kbd.setLock(leds);
      }

      // go to:
      fill_kbd_matrix(c);
    }
  }

  // transmit kbd always
  transmit_keyboard_matrix();

  // update leds on PCB
  if (n - tl >= 200) {
    digitalWrite(LED_KBD, LOW);
    blink_state = false;

    digitalWrite(LED_TURBO, is_turbo ? HIGH : LOW);
    digitalWrite(LED_SPECIAL, is_special ? HIGH: LOW);
    
  }
}

/*
 * mcu-max serial port example
 *
 * (C) 2022-2024 Gissio
 *
 * License: MIT
 */

#include <mcu-max.h>

// Modify these values to increase the AI strength:
#define MCUMAX_NODE_MAX 1000
#define MCUMAX_DEPTH_MAX 3

void print_board() {
  const char *symbols = ".PPNKBRQ.ppnkbrq";

  Serial.println("");
  Serial.println("  +-----------------+");

  for (uint32_t y = 0; y < 8; y++) {
    Serial.print(8 - y);
    Serial.print(" | ");
    for (uint32_t x = 0; x < 8; x++) {
      Serial.print(symbols[mcumax_get_piece(0x10 * y + x)]);
      Serial.print(' ');
    }
    Serial.println("|");
  }
  Serial.println("  +-----------------+");
  Serial.println("    a b c d e f g h");
  Serial.println("");
  Serial.print("Move: ");
}

void print_square(mcumax_square square)
{
  Serial.print((char)('a' + ((square & 0x07) >> 0)));
  Serial.print((char)('1' + 7 - ((square & 0x70) >> 4)));
}

void print_move(mcumax_move move) {
  if ((move.from == MCUMAX_SQUARE_INVALID) ||
    (move.to == MCUMAX_SQUARE_INVALID))
    Serial.print("(none)");
  else {
    print_square(move.from);
    print_square(move.to);
  }
}

mcumax_square get_square(char *s)
{
    mcumax_square rank = s[0] - 'a';
    if (rank > 7)
        return MCUMAX_SQUARE_INVALID;

    mcumax_square file = '8' - s[1];
    if (file > 7)
        return MCUMAX_SQUARE_INVALID;

    return 0x10 * file + rank;
}

char serial_input[5];

void init_serial_input() {
  serial_input[0] = '\0';
}

bool get_serial_input() {
  if (Serial.available())
  {
    char s[2];
    s[0] = Serial.read();
    s[1] = '\0';

    if (s[0] == '\n')
      return true;

    if (s[0] == '\b') {
      int n = strlen(s);
      if (n)
        s[n - 1] = '\0';
    }

    if (s[0] >= ' ')
    {
      if (strlen(serial_input) < 4)
        strcat(serial_input, s);

      Serial.print(s[0]);
    }
  }

  return false;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);

  init_serial_input();

  mcumax_init();

  Serial.println("mcu-max serial port example");
  Serial.println("---------------------------");
  Serial.println("");
  Serial.println("Enter moves as [from square][to square]. E.g.: e2e4");

  print_board();
}

void loop() {
  if (!get_serial_input())
    return;

  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("");

  if (!mcumax_play_move((mcumax_move){
    get_square(serial_input + 0),
    get_square(serial_input + 2),
  }))
    Serial.println("Invalid move.");
  else {
    mcumax_move move = mcumax_search_best_move(MCUMAX_NODE_MAX, MCUMAX_DEPTH_MAX);
    if (move.from == MCUMAX_SQUARE_INVALID)
      Serial.println("Game over.");
    else if (mcumax_play_move(move)) {
      Serial.print("Opponent moves: ");
      print_move(move);
      Serial.println("");
    }
  }

  init_serial_input();

  digitalWrite(LED_BUILTIN, LOW);

  print_board();
}

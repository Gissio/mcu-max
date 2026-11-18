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
  const char *symbols = ".PNBRQK?.pnbrqk?";

  Serial.println("");
  Serial.println("  +-----------------+");

  for (uint32_t y = 0; y < 8; y++) {
    Serial.print(8 - y);
    Serial.print(" | ");
    for (uint32_t x = 0; x < 8; x++) {
      Serial.print(symbols[mcumax_get_piece(MCUMAX_SQUARE(x, 7 - y))]);
      Serial.print(' ');
    }
    Serial.println("|");
  }
  Serial.println("  +-----------------+");
  Serial.println("    a b c d e f g h");
  Serial.println("");
  Serial.print("Move: ");
}

void print_square(mcumax_square square) {
  Serial.print((char)('a' + MCUMAX_SQUARE_FILE(square)));
  Serial.print((char)('1' + MCUMAX_SQUARE_RANK(square)));
}

void print_move(mcumax_move move) {
  if ((move.from == MCUMAX_SQUARE_INVALID) || (move.to == MCUMAX_SQUARE_INVALID))
    Serial.print("(none)");
  else {
    print_square(move.from);
    print_square(move.to);
  }
}

mcumax_square get_square(char *s) {
  uint8_t file = s[0] - 'a';
  uint8_t rank = s[1] - '1';
  if ((file > 7) || (rank > 7))
    return MCUMAX_SQUARE_INVALID;

  return MCUMAX_SQUARE(file, rank);
}

char serial_input[5];

void init_serial_input() {
  serial_input[0] = '\0';
}

bool get_serial_input() {
  if (Serial.available()) {
    char c = Serial.read();

    if ((c == '\n') || (c == '\r'))
      return (strlen(serial_input) != 0);

    if ((c == '\b') || (c == 0x7f)) {
      int n = strlen(serial_input);
      if (n) {
        serial_input[n - 1] = '\0';
        Serial.print("\b \b");
      }
    } else if (c >= ' ') {
      int n = strlen(serial_input);
      if (n < 4) {
        serial_input[n] = c;
        serial_input[n + 1] = '\0';
      }

      Serial.print(c);
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

// Prints the game state; returns true if the game is over
bool print_game_state() {
  switch (mcumax_get_game_state()) {
    case MCUMAX_GAME_CHECK:
      Serial.println("Check.");
      return false;

    case MCUMAX_GAME_CHECKMATE:
      Serial.println("Checkmate.");
      return true;

    case MCUMAX_GAME_STALEMATE:
      Serial.println("Stalemate.");
      return true;

    default:
      return false;
  }
}

void loop() {
  if (!get_serial_input())
    return;

  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("");

  mcumax_move move = {
    get_square(serial_input + 0),
    get_square(serial_input + 2),
  };

  // mcumax_play_move() only plays legal moves
  if (!mcumax_play_move(move))
    Serial.println("Invalid move.");
  else if (!print_game_state()) {
    move = mcumax_search_best_move(MCUMAX_NODE_MAX, MCUMAX_DEPTH_MAX);
    if (mcumax_play_move(move)) {
      Serial.print("Opponent moves: ");
      print_move(move);
      Serial.println("");

      print_game_state();
    }
  }

  init_serial_input();

  digitalWrite(LED_BUILTIN, LOW);

  print_board();
}

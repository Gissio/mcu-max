```c
// 1. Tester l'échec d'abord
bool in_check = is_in_check(side);

if (in_check) {
    // 2. Si en échec, tester le mat
    if (is_checkmate(side)) {
        // MAT - Partie terminée, défaite
    } else {
        // ÉCHEC - Doit jouer pour sortir
    }
} else {
    // 3. Si pas en échec, tester le pat
    if (is_stalemate(side)) {
        // PAT - Partie terminée, nulle
    } else {
        // NORMAL - Jeu continue
    }
}
```

##

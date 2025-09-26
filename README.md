# MCU-MAX

MCU-MAX est une bibliothèque C pour la gestion et l’analyse de parties d’échecs, conçue pour être légère, rapide et facilement intégrable dans des projets embarqués ou des applications desktop.

## Fonctionnalités principales

- Lecture et écriture de parties au format UCI
- Détection de situations de pat, échec et mat
- Interface avec Arduino (exemples fournis)
- API simple pour l’intégration dans d’autres projets

## Structure du dépôt

- `src/` : Code source principal de la bibliothèque
- `examples/` : Exemples d’utilisation (Arduino, UCI)
- `docs/` : Documentation et images
- `build/` : Fichiers générés par CMake et Make
- `tests/` : (à compléter) Tests unitaires et d’intégration

## Installation

### Prérequis

- CMake
- Un compilateur C (GCC, Clang, etc.)

### Compilation

Dans le dossier racine, exécutez :

```sh
cmake -B build
cmake --build build
```

Les binaires seront générés dans le dossier `build/`.

## Utilisation

### Exemple Arduino

Voir `examples/arduino/mcu-max-serial/mcu-max-serial.ino` pour une intégration sur microcontrôleur.

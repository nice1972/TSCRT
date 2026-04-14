# TSCRT

**Fonctionne sous Windows, Linux et macOS.**

**TSCRT** est un émulateur de terminal SSH et de console série multiplateforme, développé avec Qt 6, libssh2 et libvterm. Il s'adresse aux ingénieurs ayant besoin d'une console multisession fiable et scriptable pour travailler avec des systèmes embarqués, des équipements réseau et des serveurs distants.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | **Français** | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![Session SSH avec panneaux divisés dans TSCRT](docs/images/tscrt-split-panes.png)

---

## Fonctionnalités

### Sessions & terminal
- Sessions **SSH** propulsées par libssh2 avec keepalive, reconnexion automatique et **vérification de clé d'hôte (TOFU)**
- Sessions **Série / RS-232** pour cibles embarquées
- Gestion multi-onglets avec profils persistants
- **Type de terminal par session** (xterm-256color, vt100, etc.) avec valeur globale par défaut
- **Panneaux divisés** (horizontal/vertical) par onglet
- **Diffusion d'entrée** vers tous les panneaux d'un onglet (`Ctrl+Shift+B`)
- **Barre de recherche** avec regex et sensibilité à la casse sur tout le scrollback (`Ctrl+F`)
- Surbrillance persistante à l'échelle du terminal / fonction **Mark**
- Mode plein écran (`F11`) — tous les raccourcis restent actifs

### Multi-fenêtres
- **Détacher un onglet dans une nouvelle fenêtre** — clic droit sur un onglet et choisir « Détacher dans une nouvelle fenêtre » pour le déplacer dans une instance TSCRT distincte tout en maintenant la connexion active
- **Glisser les onglets entre fenêtres** — faire glisser un onglet depuis la barre d'une fenêtre et le déposer sur une autre
- Menu contextuel d'onglet : Renommer, Dupliquer, Épingler, Exécuter un snapshot, Détacher, Fermer

### Automatisation
- **Moteur d'automatisation** — actions au démarrage, actions déclenchées par motif (avec limite de fréquence), tâches périodiques
- **Snapshots** — exécuter des commandes shell à la connexion, sur motif ou selon un calendrier, et capturer la sortie
- **Planificateur Cron** — expressions cron à 5 champs pour le minutage des snapshots
- **Barre de boutons** — boutons d'action rapide personnalisables par session
- **Widget de ligne de commande** pour l'exécution inline d'actions / scripts

### Sécurité
- **Vérification de clé d'hôte SSH** — modèle TOFU avec fichier `known_hosts` ; les clés non concordantes sont rejetées strictement
- Identifiants chiffrés au repos : **DPAPI** (Windows), **Keychain** (macOS), repli en clair (Linux)
- Profil Windows protégé par une **ACL réservée au propriétaire**
- SMTP : **validation du certificat TLS imposée**, AUTH en clair refusé, injection d'en-têtes bloquée
- Vidages de crash : permissions réservées au propriétaire, suppression automatique après 7 jours
- Déclencheurs d'automatisation : limitation de fréquence par motif et globale
- Reconnexion : délai de base minimum de 250 ms, plafond strict de 100 tentatives

### Journalisation & fiabilité
- Enregistreur de session par panneau avec dialogue dédié aux paramètres de journal
- **Client SMTP** pour les notifications par courriel (STARTTLS/SMTPS requis pour l'authentification)
- **Gestionnaire de crash** avec prise en charge des symboles de débogage
- Reconnexion automatique avec backoff exponentiel et gigue

### Internationalisation
- Traductions UI intégrées : **English**, **한국어**, **日本語**, **中文**, **Deutsch**, **Español**, **Français**, **Italiano**, **Português**, **Русский**
- Guides d'aide HTML intégrés par langue

---

## Prérequis

| Composant | Version |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | dernière  |
| libvterm  | dernière  |
| pkg-config | requis |

**Windows** : toolchain MSYS2 UCRT64 (gcc 15+).
**macOS** : 12+ avec Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # incrémental (utiliser `clean` pour une recompilation complète)
./install.sh            # prépare exe + DLL Qt
./install.sh package    # installeur NSIS
```

## Build — macOS

Voir [`mac_build.txt`](mac_build.txt).

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop
```

---

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Suites : `test_action_parser`, `test_cron`, `test_profile`.

---

## Structure du projet

```
core/         C portable — E/S de profil, abstraction de plateforme
gui/          Qt/C++ — fenêtre principale, sessions, dialogues, automatisation, snapshots
include/      en-têtes publics partagés entre core et gui
resources/    icône, qrc, NSIS .rc, aide HTML
translations/ fichiers Qt .ts (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        tests unitaires Qt Test
```

---

## Licence

Copyright (c) 2026 TePSEG Co., Ltd. — voir [LICENSE.txt](LICENSE.txt).

Tiers : Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, runtime MinGW, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Contact

- Développeur : ygjeon@tepseg.com

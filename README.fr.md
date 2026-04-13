# TSCRT

**TSCRT** est un emulateur de terminal SSH et console serie multiplateforme, developpe avec Qt 6, libssh2 et libvterm. Concu pour les ingenieurs travaillant avec des systemes embarques, des equipements reseau et des serveurs distants.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![Session SSH avec panneaux divises dans TSCRT](docs/images/tscrt-split-panes.png)

---

## Fonctionnalites principales

- Sessions **SSH** — keepalive, reconnexion automatique, **verification de cle d'hote (TOFU)**
- Sessions **Serie / RS-232**
- Gestion multi-onglets avec profils persistants, **type de terminal par session**
- **Panneaux divises** (horizontal/vertical), **diffusion d'entree**
- **Detacher un onglet dans une nouvelle fenetre** (connexion maintenue), **glisser-deposer entre fenetres**
- **Instantanes (Snapshots)** + **planificateur Cron** + **moteur d'automatisation**
- **Securite** : verification de cle SSH, chiffrement DPAPI/Keychain, TLS SMTP obligatoire, ACL proprietaire uniquement
- Traductions UI : English, 한국어, 日本語, 中文

## Compilation

Voir [README.md](README.md) ou [`mac_build.txt`](mac_build.txt) pour les instructions detaillees.

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # Installeur NSIS
```

## Licence

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## Contact

- Developpeur : ygjeon@tepseg.com

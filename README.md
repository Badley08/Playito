<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

<p align="center">
  <img src="playito.png" alt="Playito" width="120" height="120">
</p>

<h1 align="center">Playito</h1>

<p align="center">
  Lecteur vidéo léger pour Linux — C++17, OpenGL, libmpv.
</p>

<p align="center">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cpp&logoColor=white">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-Linux-2E7D32?logo=linux&logoColor=white">
  <img alt="OpenGL" src="https://img.shields.io/badge/OpenGL-3.3-005B81?logo=opengl&logoColor=white">
  <img alt="libmpv" src="https://img.shields.io/badge/libmpv-0.41-4A90D9">
  <img alt="License" src="https://img.shields.io/badge/License-GPL%20v3-blue.svg">   
</p>

---

## Features

| | |
|---|---|
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Cpath d='M4 1v10l3-3h4l-2-2 2-2h-4l-3-3z' fill='%234FC3F7'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Drag & drop** — fichiers ou dossiers (playlist auto, triée par nom) |
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Cpolygon points='9,1 3,9 7,9 6,15 13,7 9,7' fill='%234FC3F7'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Démarrage instantané** — pas de framework GUI, pas de splash screen |
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Crect x='2' y='3' width='12' height='10' rx='1' fill='none' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='5' y1='3' x2='5' y2='13' stroke='%234FC3F7' stroke-width='1'/%3E%3Cline x1='11' y1='3' x2='11' y2='13' stroke='%234FC3F7' stroke-width='1'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Formats vidéo** : MP4, MKV, WEBM, AVI, MOV, FLV… |
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Ccircle cx='5' cy='12' r='2.5' fill='%234FC3F7'/%3E%3Crect x='7' y='3' width='1.5' height='9' fill='%234FC3F7'/%3E%3Cpath d='M7 3c3 0 5 1 5 3' stroke='%234FC3F7' stroke-width='1.5' fill='none'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Formats audio** : MP3, WAV, OGG, FLAC, AAC… |
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Cpath d='M2 6V2h4M10 2h4v4M14 10v4h-4M6 14H2v-4' stroke='%234FC3F7' stroke-width='1.5' fill='none'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Explorateur integre**, boutons lecture/pause, suivant, precedent, volume et recherche |
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Cpath d='M8 2C5 6 3 8 3 11a5 5 0 0010 0c0-3-2-5-5-9z' fill='%234FC3F7'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Dark theme** — barre de progression bleu électrique |
| <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'%3E%3Crect x='4' y='4' width='8' height='8' rx='1' fill='none' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='6' y1='1' x2='6' y2='4' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='10' y1='1' x2='10' y2='4' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='6' y1='12' x2='6' y2='15' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='10' y1='12' x2='10' y2='15' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='1' y1='6' x2='4' y2='6' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='1' y1='10' x2='4' y2='10' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='12' y1='6' x2='15' y2='6' stroke='%234FC3F7' stroke-width='1.5'/%3E%3Cline x1='12' y1='10' x2='15' y2='10' stroke='%234FC3F7' stroke-width='1.5'/%3E%3C/svg%3E" width="16" height="16" valign="middle"> | **Optimisé machines modestes** — `hwdec=auto-safe` + `profile=fast` |

---

## Stack

| Composant | Rôle |
|-----------|------|
| **C++17** | Langage |
| **OpenGL 3.3** | Rendu vidéo + UI |
| **libmpv 0.41** | Décode, rendu GPU, audio |
| **GLFW 3** | Fenêtre, input, drag & drop |

Aucun framework GUI (pas de Qt, pas de GTK, pas de SDL). L'UI est dessinée directement en OpenGL.

---

## Dépendances (Debian/Ubuntu)

```sh
sudo apt install build-essential cmake pkg-config libmpv-dev libglfw3-dev libgl1-mesa-dev libcurl4-openssl-dev libssl-dev nlohmann-json3-dev
```

---

## Compiler et lancer

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/playito
./build/playito "/chemin/vers/une_video.mkv"
```

Pour compiler, lancer les tests puis demarrer immediatement le lecteur :

```sh
bash scripts/run_local.sh
bash scripts/run_local.sh "/chemin/vers/une_video.mkv"
```

Déposer un fichier ou un dossier sur la fenêtre fonctionne aussi. Un dossier ajoute à la file tous les fichiers vidéo/audio compatibles, triés par nom.

---

## Raccourcis

| Touche | Action |
|--------|--------|
| `Espace` | Lecture / Pause |
| `←` / `→` | Reculer / Avancer 5 s |
| `↑` / `↓` | Volume + / − |
| `F` | Plein écran |
| `O` | Ouvrir l'explorateur |
| `Échap` / `Q` | Quitter |

---

## Minimum système

- Linux (X11 ou Wayland)
- GPU avec OpenGL 3.3
- AMD A4-9125 : cible de performance, tests materiels a effectuer

---

## AppImage et mises a jour

Chaque tag de version `v*` declenche une construction x86_64 et une publication
GitHub Release. A chaque demarrage de l'AppImage, Playito verifie s'il existe une
version plus recente. Le bouton `UPDATE` permet aussi de lancer cette verification
manuellement. Playito verifie le digest SHA-256 publie par GitHub et ne remplace
l'AppImage qu'apres verification. En cas de mise a jour, l'ancienne instance se
ferme et la nouvelle version est relancee automatiquement, en conservant la file
de lecture.

L'AppImage fonctionne sans installation systeme sur les distributions Linux
x86_64 compatibles. Pour ajouter Playito au menu et aux choix « Ouvrir avec »,
lance :

```sh
bash packaging/install_desktop_integration.sh "/chemin/vers/Playito-x86_64.AppImage"
```

Cette integration utilisateur ne demande pas les droits administrateur.

## License

[GNU General Public License v3.0](LICENSE)   
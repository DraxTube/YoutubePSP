# PSPTube 🎬

Clone di YouTube homebrew per Sony PSP, scritto in C con PSPSDK.

## Screenshot / Funzionalità

- 🔍 **Ricerca video** tramite tastiera on-screen
- 📈 **Video di tendenza** all'avvio
- ▶️ **Riproduzione video** in formato PSP-nativo (MPEG-4 AVC 480×272)
- ⚙️ **Impostazioni** proxy e WiFi
- 🎮 Controllo completo con i tasti PSP

---

## Architettura

```
PSP (EBOOT.PBP)
     │
     │  HTTP (WiFi)
     ▼
Proxy Server (PC / VPS)
     │
     ├── YouTube Data API v3  (ricerca, metadati)
     └── yt-dlp + ffmpeg      (transcodifica in MP4 PSP)
```

La PSP non può accedere direttamente all'API di YouTube (HTTPS moderno).
Il **proxy server** fa da intermediario e transcodifica i video.

---

## Struttura del repository

```
psp-youtube/
├── src/
│   ├── main.c        # Entry point, setup callback
│   ├── graphics.c/h  # Framebuffer, font 8×8, primitive
│   ├── font8x8.h     # Font bitmap embedded
│   ├── network.c/h   # PSP WiFi + HTTP
│   ├── youtube.c/h   # API proxy client
│   ├── ui.c/h        # Schermate, OSK, state machine
│   ├── video.c/h     # Downloader + player MPEG
│   └── config.c/h    # Lettura/scrittura config.txt
├── assets/
│   ├── ICON0.PNG     # Icona XMB 144×80
│   └── PIC1.PNG      # Sfondo XMB 480×272
├── Makefile
├── .github/
│   └── workflows/
│       └── build.yml # GitHub Actions – compila EBOOT.PBP
└── README.md
```

---

## Build con GitHub Actions (consigliato)

1. **Fork / carica** questo repo su GitHub
2. Vai su **Actions** → la build parte automaticamente ad ogni push
3. Scarica l'artefatto **PSPTube-release.zip** dalla run completata
4. Estrai e copia `PSP/GAME/PSPTube/` nella Memory Stick

### Creare una release con tag

```bash
git tag v1.0
git push origin v1.0
```

GitHub Actions creerà automaticamente una Release con lo ZIP allegato.

---

## Build manuale (locale)

### Prerequisiti

```bash
# Installa PSPDEV toolchain (Linux/macOS/WSL)
git clone https://github.com/pspdev/pspdev.git
cd pspdev
./build.sh

export PSPDEV=/usr/local/pspdev
export PATH=$PATH:$PSPDEV/bin
```

### Compilazione

```bash
cd psp-youtube
make
# Output: EBOOT.PBP
```

---

## Setup Proxy Server

Il proxy è necessario per far funzionare la ricerca e lo streaming.

### Installazione rapida (Python)

```bash
pip install flask yt-dlp requests
```

Crea `proxy.py`:

```python
from flask import Flask, jsonify, redirect
import requests, yt_dlp, os

app = Flask(__name__)
API_KEY = os.environ['YOUTUBE_API_KEY']

@app.route('/search')
def search():
    q = request.args.get('q','')
    page = request.args.get('page','')
    url = f'https://www.googleapis.com/youtube/v3/search?part=snippet&q={q}&type=video&maxResults=10&key={API_KEY}'
    if page: url += f'&pageToken={page}'
    r = requests.get(url).json()
    items = []
    for item in r.get('items',[]):
        vid = item['id']['videoId']
        title = item['snippet']['title'][:79]
        channel = item['snippet']['channelTitle'][:47]
        items.append({'id':vid,'title':title,'channel':channel,'dur':'--:--','views':'?'})
    return jsonify(items=items, next=r.get('nextPageToken',''), prev=r.get('prevPageToken',''))

@app.route('/trending')
def trending():
    url = f'https://www.googleapis.com/youtube/v3/videos?part=snippet&chart=mostPopular&regionCode=IT&maxResults=10&key={API_KEY}'
    r = requests.get(url).json()
    items = [{'id':i['id'],'title':i['snippet']['title'][:79],
              'channel':i['snippet']['channelTitle'][:47],'dur':'--:--','views':'?'}
             for i in r.get('items',[])]
    return jsonify(items=items, next='', prev='')

@app.route('/stream')
def stream():
    vid = request.args.get('id')
    ydl_opts = {
        'format': 'bestvideo[height<=272][ext=mp4]+bestaudio[ext=m4a]/best[height<=272]',
        'quiet': True,
    }
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(f'https://youtube.com/watch?v={vid}', download=False)
        return jsonify(url=info['url'])

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

```bash
export YOUTUBE_API_KEY=la_tua_chiave
python proxy.py
```

---

## Installazione sulla PSP

1. Abilita **Custom Firmware** (CFW) sulla PSP (es. PRO-C, ME)
2. Copia la cartella `PSP/GAME/PSPTube/` nella Memory Stick
3. Modifica `config.txt` con l'IP del tuo server proxy
4. Avvia da **XMB → Giochi → Memory Stick**

### config.txt

```
proxy_host=192.168.1.100   # IP del PC/server con il proxy
proxy_port=8080
wifi_config=1              # Slot WiFi PSP (1, 2 o 3)
volume=80
```

---

## Controlli

| Tasto | Funzione |
|-------|----------|
| Croce direzionale | Navigazione |
| ✕ (Cross) | Seleziona / Conferma |
| ○ (Circle) | Indietro / Annulla |
| □ (Square) | Backspace (tastiera) |
| Start | Conferma ricerca / Mostra info |
| R / L | Pagina successiva / precedente |

---

## Note tecniche

- **CPU PSP**: MIPS R4000 333MHz – clock massimo impostato
- **Formato video supportato**: MPEG-4 AVC Baseline, max 480×272, AAC audio
- **RAM disponibile**: ~20MB heap per l'app
- **Rete**: WiFi 802.11b – download ~1Mb/s reale
- Il video viene scaricato sulla Memory Stick prima della riproduzione

## Licenza

MIT – Homebrew libero, non affiliato a Sony o YouTube/Google.

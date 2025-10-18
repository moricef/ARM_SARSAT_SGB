# Conversion IQ → WAV pour COSPAS-SARSAT T.018

## Vue d'ensemble

L'outil `iq_to_wav.py` convertit les fichiers IQ générés par `sarsat_sgb` en fichiers WAV audio pour analyse avec des outils standard.

## 🎵 Formats WAV disponibles

### 1. **WAV Stéréo I/Q** (recommandé pour analyse SDR)

```bash
./tools/iq_to_wav.py test_t018.iq --stereo -o output.wav
```

**Caractéristiques:**
- Canal gauche (L): Composante **I** (In-phase)
- Canal droit (R): Composante **Q** (Quadrature)
- Format: Stéréo 16-bit, 400 kHz
- Taille: ~3 MB pour 1.92 secondes

**Usage:**
- Ouvrir dans **Audacity** pour visualiser I et Q séparément
- Analyser le spectre avec Plot Spectrum (FFT)
- Compatible avec SDR# (Import WAV as I/Q)

### 2. **WAV Mono AM** (enveloppe du signal)

```bash
./tools/iq_to_wav.py test_t018.iq --am -o output.wav
```

**Caractéristiques:**
- Magnitude: `sqrt(I² + Q²)`
- Format: Mono 16-bit, 400 kHz
- Taille: ~1.5 MB

**Usage:**
- Visualiser l'enveloppe du signal OQPSK
- Détecter les transitions de chips
- Constant envelope pour OQPSK (amplitude constante)

### 3. **WAV Mono FM** (démodulation de phase)

```bash
./tools/iq_to_wav.py test_t018.iq --fm -o output.wav
```

**Caractéristiques:**
- Dérivée de la phase: `d(angle(I+jQ))/dt`
- Format: Mono 16-bit, 400 kHz
- Taille: ~1.5 MB

**Usage:**
- Approximation de la démodulation FM
- Visualiser les transitions de phase OQPSK

---

## 📊 Utilisation

### Générer un seul format

```bash
# Stéréo I/Q (défaut)
./tools/iq_to_wav.py test_t018.iq -o output.wav

# AM uniquement
./tools/iq_to_wav.py test_t018.iq --am -o signal_am.wav

# FM uniquement
./tools/iq_to_wav.py test_t018.iq --fm -o signal_fm.wav
```

### Générer tous les formats

```bash
./tools/iq_to_wav.py test_t018.iq --all
```

Génère:
- `test_t018_stereo.wav` (3.0 MB)
- `test_t018_am.wav` (1.5 MB)
- `test_t018_fm.wav` (1.5 MB)

---

## 🔊 Écoute du signal

### ⚠️ IMPORTANT: Le signal N'EST PAS audible!

Le signal OQPSK COSPAS-SARSAT contient:
- **Chip rate**: 38.4 kHz (limite de l'audition humaine ~20 kHz)
- **Modulation numérique**: Pas de composante audio intelligible
- **Sample rate**: 400 kHz (nécessite conversion pour écoute)

### Pour "écouter" le signal:

1. **Downsampling à 48 kHz** (audio standard):
```bash
sox test_t018_stereo.wav -r 48000 test_t018_audio.wav
```

2. **Ralentir le signal** (facteur 8):
```bash
sox test_t018_stereo.wav test_t018_slow.wav speed 0.125
```

Le chip rate devient alors: 38.4 kHz / 8 = **4.8 kHz** (audible)

---

## 📈 Analyse avec Audacity

### Ouvrir le fichier

1. Lancer **Audacity**
2. Fichier → Ouvrir → `test_t018_stereo.wav`
3. Voir 2 pistes: **I** (haut) et **Q** (bas)

### Visualiser le spectre

1. Sélectionner une piste
2. Analyser → **Plot Spectrum**
3. Paramètres:
   - Algorithm: **Spectrum**
   - Function: **Hann window**
   - Size: **8192** ou plus
   - Axis: **Log frequency**

### Ce que vous devriez voir:

```
Fréquence (kHz)     Puissance
----------------------------------
0-34.56            Signal principal
34.56-69.12        Lobes secondaires (roll-off α=0.8)
>69.12             Bruit atténué
```

**Largeur de bande théorique**: (1 + α) × chip_rate / 2 = 1.8 × 38.4 / 2 = **34.56 kHz**

---

## 🔬 Analyse avancée

### Avec Python (matplotlib)

```python
import wave
import numpy as np
import matplotlib.pyplot as plt

# Lire le WAV stéréo
with wave.open('test_t018_stereo.wav', 'rb') as wav:
    frames = wav.readframes(wav.getnframes())
    data = np.frombuffer(frames, dtype=np.int16)

# Séparer I et Q
i_samples = data[0::2] / 32768.0
q_samples = data[1::2] / 32768.0

# Former les échantillons complexes
iq = i_samples + 1j * q_samples

# Calculer la FFT
fft = np.fft.fftshift(np.fft.fft(iq))
freq = np.fft.fftshift(np.fft.fftfreq(len(iq), 1/400000))

# Tracer le spectre
plt.figure(figsize=(12, 6))
plt.plot(freq/1000, 20*np.log10(np.abs(fft)))
plt.xlabel('Fréquence (kHz)')
plt.ylabel('Puissance (dB)')
plt.title('Spectre OQPSK COSPAS-SARSAT T.018')
plt.grid(True)
plt.xlim([-50, 50])
plt.show()
```

### Avec GNU Radio

```python
# Flowgraph GNU Radio Companion
File Source (WAV) → Complex to Float → QT GUI Frequency Sink
```

---

## 📁 Fichiers générés

| Fichier | Format | Taille | Usage |
|---------|--------|--------|-------|
| `test_t018.iq` | Complex float32 | 5.9 MB | Original (SDR) |
| `test_t018_stereo.wav` | Stéréo 16-bit 400kHz | 3.0 MB | Analyse I/Q |
| `test_t018_am.wav` | Mono 16-bit 400kHz | 1.5 MB | Enveloppe |
| `test_t018_fm.wav` | Mono 16-bit 400kHz | 1.5 MB | Phase |

---

## 🎯 Vérifications possibles

### 1. Constant Envelope (OQPSK)

Vérifier que la magnitude est constante:
```python
magnitude = np.abs(iq)
print(f"Magnitude min: {magnitude.min():.3f}")
print(f"Magnitude max: {magnitude.max():.3f}")
print(f"Magnitude std: {magnitude.std():.3f}")
```

Attendu: `std` très faible (< 0.05) pour OQPSK idéal

### 2. Chip Rate

Compter les transitions dans le signal FM:
- Attendu: ~38 400 chips/seconde
- Sur 1.92 s: ~73 728 chips

### 3. Data Rate

Compter les symboles (256 chips/bit):
- Attendu: 300 bps = 5 bits/seconde
- Sur 1.92 s: 576 bits ✓ (300 bits données + preamble + overhead)

---

## 🛠️ Dépannage

### Le WAV ne s'ouvre pas

- Vérifier le sample rate: 400 kHz (non standard)
- Certains lecteurs audio refusent > 192 kHz
- Solution: Downsampler avec `sox`

### Pas de signal visible

- Vérifier l'échelle Y dans Audacity (zoomer)
- Le signal est normalisé ±1.0
- Utiliser Plot Spectrum pour voir le spectre

### Signal trop rapide

- Le chip rate (38.4 kHz) est proche de la limite audio
- Ralentir avec `sox` ou analyser en FFT

---

## 📚 Références

- **T.018**: COSPAS-SARSAT specification
- **OQPSK**: Offset Quadrature Phase Shift Keying
- **Chip rate**: 38.4 kchips/s
- **Sample rate**: 400 kHz
- **Bandwidth**: ~69 kHz (avec α=0.8)

---

## ✅ Workflow complet

```bash
# 1. Générer le fichier IQ
./bin/sarsat_sgb -o test.iq -t 0 -lat 43.2 -lon 5.4

# 2. Convertir en WAV (tous formats)
./tools/iq_to_wav.py test.iq --all

# 3. Downsampler pour audio standard
sox test_stereo.wav -r 48000 test_audio.wav

# 4. Analyser dans Audacity
audacity test_audio.wav

# 5. Ou analyser le spectre avec Python
python3 analyze_spectrum.py test_stereo.wav
```

**Résultat**: Visualisation complète du signal OQPSK T.018 conforme! ✅

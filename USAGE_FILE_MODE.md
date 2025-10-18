# SARSAT_SGB - Mode Génération de Fichier IQ

## Vue d'ensemble

Le mode fichier permet de générer des fichiers IQ pour validation SANS PlutoSDR connecté.

## Utilisation

### Générer un fichier IQ

```bash
./bin/sarsat_sgb -o output.iq [options]
```

### Exemples

```bash
# EPIRB standard (France, test mode)
./bin/sarsat_sgb -o epirb_test.iq -t 0 -lat 43.2 -lon 5.4

# PLB (Personal Locator Beacon)
./bin/sarsat_sgb -o plb_test.iq -t 1 -c 227 -s 12345 -lat 45.0 -lon 6.0

# ELT (Emergency Locator Transmitter)
./bin/sarsat_sgb -o elt_test.iq -t 2 -alt 1500 -lat 46.5 -lon 7.2

# ELT-DT (ELT avec homing)
./bin/sarsat_sgb -o elt_dt_test.iq -t 3 -lat 47.0 -lon 8.0
```

## Options disponibles

| Option | Description | Défaut |
|--------|-------------|--------|
| `-o <file>` | Fichier IQ de sortie | (obligatoire) |
| `-t <type>` | Type: 0=EPIRB, 1=PLB, 2=ELT, 3=ELT-DT | 0 |
| `-c <code>` | Code pays (MID) | 227 (France) |
| `-s <serial>` | Numéro de série | 13398 |
| `-m <mode>` | Mode test: 0=Exercise, 1=Test | 1 |
| `-lat <lat>` | Latitude (degrés) | 43.2 |
| `-lon <lon>` | Longitude (degrés) | 5.4 |
| `-alt <alt>` | Altitude (mètres) | 0 |

## Caractéristiques du fichier IQ

- **Format**: Complex float32 (I/Q entrelacés)
- **Taux d'échantillonnage**: 400 kHz
- **Nombre d'échantillons**: 768 000
- **Durée**: 1.92 secondes
- **Taille du fichier**: ~6 Mo

### Structure des données

```
[I0, Q0, I1, Q1, I2, Q2, ..., I767999, Q767999]
```

Chaque valeur est un `float` (4 octets), amplitude normalisée ±1.0.

## Analyse du signal

### Avec GNU Radio

```python
# GNU Radio Companion
File Source -> Complex type, sample rate = 400e3
```

### Avec inspectrum

```bash
inspectrum output.iq
# Définir: Sample rate = 400000 Hz
```

### Avec URH (Universal Radio Hacker)

```bash
urh output.iq
# Sélectionner: Sample rate = 400 kHz, FSK/OQPSK
```

## Validation T.018

### Vérifications attendues

1. **Modulation**: OQPSK avec offset Tc/2
2. **Chip rate**: 38.4 kchips/s
3. **Étalement**: DSSS 256 chips/bit
4. **Durée de trame**: 300 bits × (256/38400) = 2.0 secondes
5. **Spectre**: Largeur ~69 kHz (avec α=0.8 roll-off)

### Spectrogramme attendu

- **Bande passante**: ~34.56 kHz par lobe
- **Puissance moyenne**: ~1.5 (normalized)
- **PAPR**: < 3 dB (OQPSK constant envelope)

## Comparaison avec transmission PlutoSDR

| Caractéristique | Mode Fichier | Mode PlutoSDR |
|----------------|--------------|---------------|
| **PlutoSDR requis** | ❌ Non | ✅ Oui |
| **Génère fichier** | ✅ Oui | ❌ Non |
| **Transmission RF** | ❌ Non | ✅ Oui |
| **Usage** | Validation | Production |
| **Boucle infinie** | ❌ Non (1 trame) | ✅ Oui (périodique) |

## Dépannage

### Erreur "Segmentation fault"

Si vous obtenez un segfault, c'est probablement dû à un buffer trop petit.

Vérifier dans `include/oqpsk_modulator.h`:
```c
#define OQPSK_TOTAL_SAMPLES  960000  // Doit être ≥ 768000
```

### Fichier non créé

- Vérifier les permissions d'écriture dans le répertoire
- Essayer avec un chemin absolu: `-o /tmp/test.iq`
- Vérifier l'espace disque disponible (besoin de ~6 Mo)

### Validation échouée

Si `oqpsk_verify_output()` échoue:
- Vérifier les valeurs I/Q: doivent être dans [-1.5, +1.5]
- Vérifier la puissance moyenne: doit être ~1.5
- Vérifier qu'il n'y a pas de NaN ou Inf

## Prochaines étapes

1. **Analyser le spectre** avec GNU Radio ou inspectrum
2. **Vérifier la conformité T.018** (masque spectral)
3. **Comparer avec FGB** (1ère génération)
4. **Tester avec PlutoSDR** physique
5. **Ajouter filtre RRC** dans FPGA pour conformité complète

## Référence

- Spécification: COSPAS-SARSAT T.018
- Fréquence: 406.028 MHz (production) / 403 MHz (test)
- Modulation: OQPSK + DSSS
- Débit: 300 bps (150 bps par canal I/Q)

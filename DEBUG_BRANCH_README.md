# DEBUG BRANCH: dump-chips-raw

**Date:** 2025-10-26
**Branche:** `debug/dump-chips-raw`
**Objectif:** Instrumenter le modulateur OQPSK pour dumper les chips bruts après spreading DSSS

## Modifications

### Fichier: `src/oqpsk_modulator.c`

**Ligne 205-215:** Ajout d'un dump des chips I/Q après spreading DSSS, **avant** l'interpolation OQPSK.

```c
// DEBUG: Dump chips after spreading (before interpolation)
FILE *chip_dump = fopen("chips_after_spreading.bin", "wb");
if (chip_dump) {
    // Format: interleaved I/Q chips as int8_t
    for (int i = 0; i < 38400; i++) {
        fwrite(&i_prn[i], sizeof(int8_t), 1, chip_dump);
        fwrite(&q_prn[i], sizeof(int8_t), 1, chip_dump);
    }
    fclose(chip_dump);
    printf("  [DEBUG] Chips dumped to chips_after_spreading.bin (76,800 bytes)\n");
}
```

## Format du fichier généré

**Fichier:** `chips_after_spreading.bin`
**Taille:** 76,800 octets (38,400 chips I/Q)
**Format:** Interleaved I/Q, int8_t values (+1 ou -1)

```
Byte 0:    I_chip[0]
Byte 1:    Q_chip[0]
Byte 2:    I_chip[1]
Byte 3:    Q_chip[1]
...
Byte 76798: I_chip[38399]
Byte 76799: Q_chip[38399]
```

## Utilisation

### Générer le signal avec dump des chips

```bash
cd /home/fab2/Developpement/COSPAS-SARSAT/ADALM-PLUTO/SARSAT_SGB
make clean && make
./bin/sarsat_sgb -o test_with_chip_dump.iq
```

Deux fichiers seront créés:
- `test_with_chip_dump.iq` - Signal IQ complet (2.5 MHz, 2.5M samples)
- `chips_after_spreading.bin` - Chips bruts (38.4 kHz, 38.4k chips)

### Lire le fichier de chips

```python
import numpy as np

chips = np.fromfile('chips_after_spreading.bin', dtype=np.int8)
i_chips = chips[0::2]  # Chips I
q_chips = chips[1::2]  # Chips Q

print(f"I chips: {len(i_chips)}")
print(f"Q chips: {len(q_chips)}")
print(f"First 10 I: {i_chips[:10]}")
print(f"First 10 Q: {q_chips[:10]}")
```

## Retour à la version originale

### Option 1: Revenir à master

```bash
git checkout master
```

### Option 2: Supprimer la branche debug

```bash
git checkout master
git branch -D debug/dump-chips-raw
```

### Option 3: Retirer juste le dump (garder la branche)

Éditer `src/oqpsk_modulator.c`, lignes 205-215, supprimer le bloc `// DEBUG: Dump chips...`

## Objectif de cette instrumentation

Cette branche permet de:

1. **Vérifier le TX:** Comparer les chips générés avec les valeurs attendues
2. **Vérifier le RX:** Comparer les chips extraits du signal IQ avec les chips de référence
3. **Isoler le problème:** Déterminer si l'erreur vient du spreading, de la modulation, ou de la démodulation

## Notes

- Les chips sont au format **int8_t** (-1 ou +1), pas des floats
- L'ordre est **interleaved I/Q**, pas séparé
- Le dump se fait **avant** OQPSK, donc pas de délai Tc/2 entre I et Q dans ce fichier
- Ce fichier n'est généré que lors de l'exécution, pas à la compilation

## Commit associé

Voir: `git log --oneline --graph debug/dump-chips-raw`

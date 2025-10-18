#!/usr/bin/env python3
"""
Convertisseur IQ → WAV pour fichiers COSPAS-SARSAT T.018

Modes disponibles:
1. Stéréo I/Q: Canal gauche = I, canal droit = Q
2. Mono FM: Démodulation FM du signal OQPSK
3. Mono AM: Enveloppe du signal (magnitude)
"""

import numpy as np
import sys
import struct
import wave
import argparse

def read_iq_file(filename):
    """Lit un fichier IQ (float32 interleaved I/Q)"""
    print(f"📖 Lecture du fichier IQ: {filename}")

    with open(filename, 'rb') as f:
        data = f.read()

    # Convertir bytes → float32 array
    num_floats = len(data) // 4
    iq_data = struct.unpack(f'{num_floats}f', data)

    # Séparer I et Q
    i_samples = np.array(iq_data[0::2], dtype=np.float32)
    q_samples = np.array(iq_data[1::2], dtype=np.float32)

    # Former les échantillons complexes
    complex_samples = i_samples + 1j * q_samples

    num_samples = len(complex_samples)
    duration = num_samples / 400000.0

    print(f"  Échantillons: {num_samples}")
    print(f"  Durée: {duration:.3f} secondes")
    print(f"  Taux: 400 kHz")

    return complex_samples, i_samples, q_samples

def write_wav_stereo_iq(filename, i_samples, q_samples, sample_rate=400000):
    """Écrit un fichier WAV stéréo (I sur gauche, Q sur droite)"""
    print(f"\n💾 Création WAV stéréo I/Q: {filename}")

    # Normaliser à ±1.0
    i_norm = i_samples / np.max(np.abs(i_samples))
    q_norm = q_samples / np.max(np.abs(q_samples))

    # Convertir en int16 (±32767)
    i_int16 = (i_norm * 32767).astype(np.int16)
    q_int16 = (q_norm * 32767).astype(np.int16)

    # Entrelacer I et Q pour stéréo
    stereo_data = np.empty((len(i_int16) * 2,), dtype=np.int16)
    stereo_data[0::2] = i_int16  # Canal gauche = I
    stereo_data[1::2] = q_int16  # Canal droit = Q

    # Écrire fichier WAV
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(2)  # Stéréo
        wav_file.setsampwidth(2)  # 16 bits
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(stereo_data.tobytes())

    print(f"  ✓ WAV créé: {len(i_int16)} échantillons")
    print(f"  Format: Stéréo 16-bit, {sample_rate} Hz")
    print(f"  Durée: {len(i_int16)/sample_rate:.3f} secondes")

def write_wav_mono_am(filename, complex_samples, sample_rate=400000):
    """Écrit un fichier WAV mono (enveloppe AM = magnitude)"""
    print(f"\n💾 Création WAV mono AM (enveloppe): {filename}")

    # Calculer la magnitude (enveloppe)
    magnitude = np.abs(complex_samples)

    # Normaliser
    magnitude_norm = magnitude / np.max(magnitude)

    # Convertir en int16
    audio_int16 = (magnitude_norm * 32767).astype(np.int16)

    # Écrire fichier WAV
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16 bits
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_int16.tobytes())

    print(f"  ✓ WAV créé: {len(audio_int16)} échantillons")
    print(f"  Format: Mono 16-bit, {sample_rate} Hz")
    print(f"  Durée: {len(audio_int16)/sample_rate:.3f} secondes")

def write_wav_mono_fm(filename, complex_samples, sample_rate=400000):
    """Écrit un fichier WAV mono (démodulation FM approximative)"""
    print(f"\n💾 Création WAV mono FM (démodulation): {filename}")

    # Démodulation FM: dérivée de la phase
    # angle(sample[n]) - angle(sample[n-1])
    phase = np.angle(complex_samples)

    # Calculer la différence de phase (dérivée)
    phase_diff = np.diff(phase)

    # Unwrap pour éviter les sauts de phase
    phase_diff = np.unwrap(phase_diff)

    # Normaliser
    if np.max(np.abs(phase_diff)) > 0:
        fm_signal = phase_diff / np.max(np.abs(phase_diff))
    else:
        fm_signal = phase_diff

    # Convertir en int16
    audio_int16 = (fm_signal * 32767).astype(np.int16)

    # Écrire fichier WAV
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16 bits
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_int16.tobytes())

    print(f"  ✓ WAV créé: {len(audio_int16)} échantillons")
    print(f"  Format: Mono 16-bit, {sample_rate} Hz")
    print(f"  Durée: {len(audio_int16)/sample_rate:.3f} secondes")

def main():
    parser = argparse.ArgumentParser(
        description='Convertit un fichier IQ en WAV audio',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemples:
  # WAV stéréo I/Q (compatible SDR#, audacity)
  %(prog)s test.iq -o output.wav --stereo

  # WAV mono enveloppe AM
  %(prog)s test.iq -o output.wav --am

  # WAV mono démodulation FM
  %(prog)s test.iq -o output.wav --fm

  # Tous les formats
  %(prog)s test.iq --all
        """
    )

    parser.add_argument('input', help='Fichier IQ source (.iq)')
    parser.add_argument('-o', '--output', help='Fichier WAV de sortie')
    parser.add_argument('--stereo', action='store_true',
                       help='WAV stéréo I/Q (I=gauche, Q=droite)')
    parser.add_argument('--am', action='store_true',
                       help='WAV mono enveloppe AM')
    parser.add_argument('--fm', action='store_true',
                       help='WAV mono démodulation FM')
    parser.add_argument('--all', action='store_true',
                       help='Générer tous les formats')
    parser.add_argument('--rate', type=int, default=400000,
                       help='Taux d\'échantillonnage (défaut: 400000 Hz)')

    args = parser.parse_args()

    # Lecture du fichier IQ
    try:
        complex_samples, i_samples, q_samples = read_iq_file(args.input)
    except FileNotFoundError:
        print(f"❌ Erreur: Fichier '{args.input}' non trouvé")
        return 1
    except Exception as e:
        print(f"❌ Erreur lors de la lecture: {e}")
        return 1

    # Déterminer les formats à générer
    if args.all:
        do_stereo = True
        do_am = True
        do_fm = True
    else:
        do_stereo = args.stereo
        do_am = args.am
        do_fm = args.fm

        # Si aucun format spécifié, faire stéréo par défaut
        if not (do_stereo or do_am or do_fm):
            do_stereo = True

    # Nom de base pour les fichiers de sortie
    if args.output:
        base_name = args.output.replace('.wav', '')
    else:
        base_name = args.input.replace('.iq', '')

    # Générer les fichiers demandés
    try:
        if do_stereo:
            output_file = f"{base_name}_stereo.wav" if args.all else f"{base_name}.wav"
            write_wav_stereo_iq(output_file, i_samples, q_samples, args.rate)

        if do_am:
            output_file = f"{base_name}_am.wav" if args.all else f"{base_name}.wav"
            write_wav_mono_am(output_file, complex_samples, args.rate)

        if do_fm:
            output_file = f"{base_name}_fm.wav" if args.all else f"{base_name}.wav"
            write_wav_mono_fm(output_file, complex_samples, args.rate)

        print("\n✅ Conversion terminée avec succès!")
        print("\n💡 Conseils d'écoute:")
        print("   - Le signal OQPSK n'est PAS audible directement")
        print("   - Utilisez un logiciel SDR pour visualiser le spectre")
        print("   - Audacity: Analyser → Plot Spectrum (FFT)")
        print("   - Le chip rate (38.4 kHz) est dans la bande audio")

        return 0

    except Exception as e:
        print(f"\n❌ Erreur lors de la conversion: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())

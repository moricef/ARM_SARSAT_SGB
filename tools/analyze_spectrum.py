#!/usr/bin/env python3
"""
Analyseur de spectre pour fichiers WAV COSPAS-SARSAT T.018
Affiche le spectre, la constellation OQPSK, et le diagramme œil
"""

import numpy as np
import matplotlib.pyplot as plt
import wave
import sys
import argparse

def read_wav_iq(filename):
    """Lit un fichier WAV stéréo I/Q"""
    with wave.open(filename, 'rb') as wav:
        n_channels = wav.getnchannels()
        sample_rate = wav.getframerate()
        n_frames = wav.getnframes()

        frames = wav.readframes(n_frames)
        data = np.frombuffer(frames, dtype=np.int16)

    if n_channels == 2:
        # Stéréo: I sur gauche, Q sur droite
        i_samples = data[0::2] / 32768.0
        q_samples = data[1::2] / 32768.0
    else:
        # Mono: utiliser comme I, Q = 0
        i_samples = data / 32768.0
        q_samples = np.zeros_like(i_samples)

    # Former les échantillons complexes
    iq = i_samples + 1j * q_samples

    return iq, sample_rate

def plot_spectrum(iq, sample_rate):
    """Trace le spectre de puissance"""
    # Calculer la FFT
    fft = np.fft.fftshift(np.fft.fft(iq))
    freq = np.fft.fftshift(np.fft.fftfreq(len(iq), 1/sample_rate))

    # Puissance en dB
    power_db = 20 * np.log10(np.abs(fft) + 1e-10)

    plt.subplot(2, 2, 1)
    plt.plot(freq/1000, power_db, linewidth=0.5)
    plt.xlabel('Fréquence (kHz)')
    plt.ylabel('Puissance (dB)')
    plt.title('Spectre de puissance OQPSK')
    plt.grid(True, alpha=0.3)
    plt.xlim([-80, 80])

    # Marquer les bandes théoriques
    chip_rate = 38.4  # kHz
    alpha = 0.8
    bw_main = (1 + alpha) * chip_rate / 2
    plt.axvline(-bw_main, color='r', linestyle='--', alpha=0.5, label=f'BW = ±{bw_main:.1f} kHz')
    plt.axvline(+bw_main, color='r', linestyle='--', alpha=0.5)
    plt.legend()

def plot_constellation(iq, max_points=10000):
    """Trace le diagramme de constellation OQPSK"""
    # Sous-échantillonner pour affichage
    step = max(1, len(iq) // max_points)
    iq_sample = iq[::step]

    plt.subplot(2, 2, 2)
    plt.plot(np.real(iq_sample), np.imag(iq_sample), '.', markersize=1, alpha=0.3)
    plt.xlabel('I (In-phase)')
    plt.ylabel('Q (Quadrature)')
    plt.title('Constellation OQPSK')
    plt.grid(True, alpha=0.3)
    plt.axis('equal')

    # Marquer les points théoriques OQPSK (±1, ±1)
    plt.plot([-1, 1, -1, 1], [-1, -1, 1, 1], 'rx', markersize=10, label='Symboles théoriques')
    plt.legend()

def plot_eye_diagram(iq, samples_per_symbol=2667):
    """Trace le diagramme œil (Eye diagram)"""
    # Extraire la partie réelle (I channel)
    signal = np.real(iq)

    # Nombre de symboles à superposer
    n_symbols = len(signal) // samples_per_symbol
    n_symbols = min(n_symbols, 100)  # Limiter pour la lisibilité

    plt.subplot(2, 2, 3)

    for i in range(n_symbols):
        start = i * samples_per_symbol
        end = start + samples_per_symbol
        if end <= len(signal):
            plt.plot(signal[start:end], 'b-', alpha=0.1, linewidth=0.5)

    plt.xlabel('Échantillons par symbole')
    plt.ylabel('Amplitude I')
    plt.title('Diagramme œil (I channel)')
    plt.grid(True, alpha=0.3)

def plot_time_domain(iq, sample_rate, duration=0.01):
    """Trace le signal temporel (I et Q)"""
    # Limiter à duration secondes pour visibilité
    n_samples = int(duration * sample_rate)
    n_samples = min(n_samples, len(iq))

    time = np.arange(n_samples) / sample_rate * 1000  # en ms

    plt.subplot(2, 2, 4)
    plt.plot(time, np.real(iq[:n_samples]), 'b-', label='I', linewidth=0.5, alpha=0.7)
    plt.plot(time, np.imag(iq[:n_samples]), 'r-', label='Q', linewidth=0.5, alpha=0.7)
    plt.xlabel('Temps (ms)')
    plt.ylabel('Amplitude')
    plt.title(f'Signal temporel (premiers {duration*1000:.0f} ms)')
    plt.grid(True, alpha=0.3)
    plt.legend()

def print_statistics(iq, sample_rate):
    """Affiche les statistiques du signal"""
    magnitude = np.abs(iq)
    phase = np.angle(iq)

    print("\n" + "="*70)
    print(" STATISTIQUES DU SIGNAL T.018")
    print("="*70)

    print(f"\n📊 Paramètres généraux:")
    print(f"  Échantillons:       {len(iq)}")
    print(f"  Taux:               {sample_rate/1000:.0f} kHz")
    print(f"  Durée:              {len(iq)/sample_rate:.3f} secondes")

    print(f"\n📈 Composante I (In-phase):")
    i_samples = np.real(iq)
    print(f"  Min:                {i_samples.min():+.6f}")
    print(f"  Max:                {i_samples.max():+.6f}")
    print(f"  Moyenne:            {i_samples.mean():+.6f}")
    print(f"  Écart-type:         {i_samples.std():.6f}")

    print(f"\n📉 Composante Q (Quadrature):")
    q_samples = np.imag(iq)
    print(f"  Min:                {q_samples.min():+.6f}")
    print(f"  Max:                {q_samples.max():+.6f}")
    print(f"  Moyenne:            {q_samples.mean():+.6f}")
    print(f"  Écart-type:         {q_samples.std():.6f}")

    print(f"\n📐 Magnitude (Enveloppe):")
    print(f"  Min:                {magnitude.min():.6f}")
    print(f"  Max:                {magnitude.max():.6f}")
    print(f"  Moyenne:            {magnitude.mean():.6f}")
    print(f"  Écart-type:         {magnitude.std():.6f}")
    print(f"  Variation (PAPR):   {(magnitude.max()/magnitude.mean()):.3f}")

    # Vérification OQPSK constant envelope
    if magnitude.std() < 0.1:
        print(f"  ✅ Enveloppe constante (OQPSK conforme)")
    else:
        print(f"  ⚠️  Enveloppe variable (PAPR élevé)")

    print(f"\n🌀 Phase:")
    print(f"  Min:                {np.degrees(phase.min()):+.1f}°")
    print(f"  Max:                {np.degrees(phase.max()):+.1f}°")
    print(f"  Transitions:        ~{len(np.where(np.abs(np.diff(phase)) > 1)[0])} (phase jumps)")

    print("\n" + "="*70)

def main():
    parser = argparse.ArgumentParser(
        description='Analyse le spectre d\'un fichier WAV COSPAS-SARSAT',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('wav_file', help='Fichier WAV à analyser')
    parser.add_argument('--save', help='Sauvegarder les graphiques (PNG)', metavar='output.png')
    parser.add_argument('--no-display', action='store_true', help='Ne pas afficher (juste sauvegarder)')

    args = parser.parse_args()

    # Lire le fichier WAV
    try:
        print(f"📖 Lecture du fichier: {args.wav_file}")
        iq, sample_rate = read_wav_iq(args.wav_file)
        print(f"  ✓ {len(iq)} échantillons @ {sample_rate/1000:.0f} kHz")
    except FileNotFoundError:
        print(f"❌ Erreur: Fichier '{args.wav_file}' non trouvé")
        return 1
    except Exception as e:
        print(f"❌ Erreur lors de la lecture: {e}")
        return 1

    # Afficher les statistiques
    print_statistics(iq, sample_rate)

    # Créer les graphiques
    print("\n📊 Génération des graphiques...")
    plt.figure(figsize=(14, 10))

    plot_spectrum(iq, sample_rate)
    plot_constellation(iq)
    plot_eye_diagram(iq)
    plot_time_domain(iq, sample_rate)

    plt.tight_layout()

    # Sauvegarder ou afficher
    if args.save:
        plt.savefig(args.save, dpi=150, bbox_inches='tight')
        print(f"  ✓ Graphiques sauvegardés: {args.save}")

    if not args.no_display:
        print("\n🖼️  Affichage des graphiques (fermer la fenêtre pour continuer)...")
        plt.show()

    print("\n✅ Analyse terminée!")
    return 0

if __name__ == '__main__':
    sys.exit(main())

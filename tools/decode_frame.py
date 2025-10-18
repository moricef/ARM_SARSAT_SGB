#!/usr/bin/env python3
"""
Décodeur de trame T.018 COSPAS-SARSAT
Analyse le contenu complet d'une trame générée par sarsat_sgb
"""

import sys

def bits_to_int(bits):
    """Convertit une liste de bits en entier"""
    value = 0
    for bit in bits:
        value = (value << 1) | bit
    return value

def decode_15hex_id(frame_bits):
    """Décode l'identifiant 15 HEX (40 bits)"""
    # Bits 2-41: TAC (10 bits) + Serial (30 bits)
    id_bits = frame_bits[2:42]

    tac = bits_to_int(id_bits[0:10])
    serial = bits_to_int(id_bits[10:40])

    # Dérivation du code pays depuis le serial (derniers 10 bits)
    country = serial & 0x3FF

    return tac, serial, country

def decode_location_protocol(frame_bits):
    """Décode le protocole de localisation (3 bits)"""
    loc_bits = frame_bits[42:45]
    protocol = bits_to_int(loc_bits)

    protocols = {
        0b000: "Standard Location (GPS)",
        0b001: "National Location",
        0b010: "ELT(DT) Location",
        0b011: "RLS Location",
        0b100: "Test User",
        0b101: "EPIRB with AIS",
        0b110: "Reserved",
        0b111: "National Use"
    }

    return protocol, protocols.get(protocol, "Unknown")

def decode_gps_position(frame_bits):
    """Décode la position GPS (60 bits)"""
    # Latitude (21 bits) - bits 45-65
    lat_bits = frame_bits[45:66]
    lat_raw = bits_to_int(lat_bits)

    # T.018 Appendix C: latitude = (raw - 1048576) / 11930.46
    latitude = (lat_raw - 1048576) / 11930.46

    # Longitude (22 bits) - bits 66-87
    lon_bits = frame_bits[66:88]
    lon_raw = bits_to_int(lon_bits)

    # T.018 Appendix C: longitude = (raw - 2097152) / 11930.46
    longitude = (lon_raw - 2097152) / 11930.46

    return latitude, longitude

def decode_supplementary_data(frame_bits):
    """Décode les données supplémentaires (112 bits)"""
    # Bits 88-199: données supplémentaires
    supp_bits = frame_bits[88:200]

    # Type de balise (4 bits) - bits 88-91
    beacon_type_bits = supp_bits[0:4]
    beacon_type = bits_to_int(beacon_type_bits)

    beacon_types = {
        0b0000: "EPIRB",
        0b0001: "PLB",
        0b0010: "ELT",
        0b0011: "ELT-DT"
    }

    return beacon_type, beacon_types.get(beacon_type, "Unknown")

def decode_frame(hex_string):
    """Décode une trame T.018 complète depuis sa représentation hexadécimale"""

    # Convertir hex en bits
    frame_bits = []
    for hex_char in hex_string.replace(" ", "").replace("\n", ""):
        nibble = int(hex_char, 16)
        for i in range(3, -1, -1):
            frame_bits.append((nibble >> i) & 1)

    # Limiter à 252 bits (trame complète avec BCH)
    frame_bits = frame_bits[:252]

    print("=" * 70)
    print(" DÉCODAGE COMPLET DE LA TRAME T.018 COSPAS-SARSAT")
    print("=" * 70)

    # Header (2 bits)
    print("\n📋 HEADER (bits 0-1)")
    print("-" * 70)
    format_flag = frame_bits[0]
    protocol_flag = frame_bits[1]
    print(f"  Format Flag:    {format_flag} ({'User' if format_flag == 1 else 'Location'})")
    print(f"  Protocol Flag:  {protocol_flag} ({'Standard' if protocol_flag == 0 else 'National'})")

    # 15 HEX ID (40 bits)
    print("\n🆔 IDENTIFIANT 15 HEX (bits 2-41)")
    print("-" * 70)
    tac, serial, country = decode_15hex_id(frame_bits)
    print(f"  TAC Number:     {tac} (0x{tac:03X})")
    print(f"  Serial Number:  {serial} (0x{serial:08X})")
    print(f"  Country Code:   {country} (MID)")

    # 15 HEX ID complet
    hex_id = (tac << 30) | serial
    hex_id_str = f"{hex_id:010X}"
    print(f"  15 HEX ID:      {hex_id_str}")

    # Location Protocol (3 bits)
    print("\n📍 PROTOCOLE DE LOCALISATION (bits 42-44)")
    print("-" * 70)
    protocol_code, protocol_name = decode_location_protocol(frame_bits)
    print(f"  Code:           {protocol_code:03b} ({protocol_code})")
    print(f"  Type:           {protocol_name}")

    # Position GPS (60 bits)
    print("\n🌍 POSITION GPS (bits 45-87)")
    print("-" * 70)
    latitude, longitude = decode_gps_position(frame_bits)
    print(f"  Latitude:       {latitude:+.6f}°")
    print(f"  Longitude:      {longitude:+.6f}°")

    # Latitude raw
    lat_raw = bits_to_int(frame_bits[45:66])
    lon_raw = bits_to_int(frame_bits[66:88])
    print(f"  Latitude (raw): {lat_raw} (0x{lat_raw:05X})")
    print(f"  Longitude (raw):{lon_raw} (0x{lon_raw:06X})")

    # Données supplémentaires
    print("\n📦 DONNÉES SUPPLÉMENTAIRES (bits 88-199)")
    print("-" * 70)
    beacon_type_code, beacon_type_name = decode_supplementary_data(frame_bits)
    print(f"  Type de balise: {beacon_type_code:04b} ({beacon_type_name})")

    # Bits 92-199: autres données (altitude, homing, etc.)
    print(f"  Autres données: {112-4} bits (détail dépend du type)")

    # National Use (2 bits)
    print("\n🏳️  USAGE NATIONAL (bits 200-201)")
    print("-" * 70)
    national_bits = frame_bits[200:202]
    national_use = bits_to_int(national_bits)
    print(f"  Valeur:         {national_use:02b} ({national_use})")

    # BCH (48 bits)
    print("\n🔒 CODE CORRECTEUR BCH (bits 202-249)")
    print("-" * 70)
    bch_bits = frame_bits[202:250]
    bch_value = bits_to_int(bch_bits)
    print(f"  BCH(250,202):   0x{bch_value:012X}")
    print(f"  Générateur:     0x1C7EB85DF3C97")

    # Padding (2 bits)
    print("\n⚙️  PADDING (bits 250-251)")
    print("-" * 70)
    padding = frame_bits[250:252]
    print(f"  Valeur:         {padding[0]}{padding[1]}")

    # Résumé
    print("\n" + "=" * 70)
    print(" RÉSUMÉ DE LA TRAME")
    print("=" * 70)
    print(f"  Type:           {beacon_type_name}")
    print(f"  15 HEX ID:      {hex_id_str}")
    print(f"  Position:       {latitude:+.6f}°, {longitude:+.6f}°")
    print(f"  Protocole:      {protocol_name}")
    print(f"  Taille totale:  252 bits (63 hex chars)")
    print("=" * 70)

if __name__ == "__main__":
    # Trame extraite de la sortie du programme
    frame_hex = """
    89C3F45638D95999
    A02B33326C3EC440
    0003FFF00C028320
    000E899A09C80A4
    """

    decode_frame(frame_hex)

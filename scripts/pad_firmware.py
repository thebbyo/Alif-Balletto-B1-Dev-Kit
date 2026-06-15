import os
import sys
import argparse

def pad_file(filepath):
    """
    Pads a binary file to ensure its size is a perfect multiple of 16 bytes.
    This is required by the Alif Secure Enclave MRAM flashing utility.
    """
    if not os.path.exists(filepath):
        print(f"[!] Error: File '{filepath}' not found.")
        sys.exit(1)

    with open(filepath, 'rb') as f:
        data = f.read()

    original_size = len(data)
    remainder = original_size % 16

    if remainder == 0:
        print(f"[+] File is already {original_size} bytes (multiple of 16). No padding needed.")
        return

    pad_length = 16 - remainder
    padded_data = data + (b'\x00' * pad_length)

    with open(filepath, 'wb') as f:
        f.write(padded_data)

    print(f"[+] Successfully padded file!")
    print(f"    Original Size: {original_size} bytes")
    print(f"    Added: {pad_length} bytes of zero-padding")
    print(f"    New Size: {len(padded_data)} bytes")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Pad a binary file to a multiple of 16 bytes for Alif MRAM flashing.")
    parser.add_argument('file', help="Path to the binary file (e.g., zephyr.bin)")
    args = parser.parse_args()
    
    pad_file(args.file)

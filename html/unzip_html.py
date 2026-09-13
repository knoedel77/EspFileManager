import gzip
import re

# Datei mit dem Header einlesen
with open("webpage_h.txt", "r") as f:
    content = f.read()

# Alle Hex-Bytes aus dem Array extrahieren (0x.. Muster)
hex_bytes = re.findall(r'0x([0-9a-fA-F]{2})', content)
data = bytes(int(b, 16) for b in hex_bytes)

# gzip dekomprimieren
html = gzip.decompress(data).decode('utf-8')

# In Datei schreiben
with open("webpage.html", "w", encoding="utf-8") as f:
    f.write(html)

print("Dekomprimiert nach webpage.html")
print(f"Größe: {len(html)} Bytes")
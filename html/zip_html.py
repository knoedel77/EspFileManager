import gzip
import textwrap

with open("webpage.html", "rb") as f:
    html = f.read()

# Komprimieren (mtime=0 für reproduzierbare Ausgabe)
compressed = gzip.compress(html, mtime=0)

# Byte-Array formatieren
lines = []
for i in range(0, len(compressed), 12):
    chunk = compressed[i:i+12]
    lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ", ")

array_body = "\n".join(lines)

output = f"""#ifndef WEBPAGE_H
#define WEBPAGE_H

const size_t html_page_len = {len(compressed)};
const uint8_t html_page[] = {{
{array_body.rstrip().rstrip(',')}
}};

#endif
"""

with open("webpage_new.h", "w") as f:
    f.write(output)

print(f"Kompiliert: {len(html)} -> {len(compressed)} Bytes")
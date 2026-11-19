#!/usr/bin/env python3
import math, struct, zlib, sys
W = H = 512
rows = []
for y in range(H):
    row = bytearray([0])
    for x in range(W):
        dx, dy = x - 256, y - 256
        r = int(9 + 8 * y / H)
        g = int(20 + 12 * y / H)
        b = int(42 + 24 * y / H)
        # A simple original neural-network mark.
        pts = [(-125,-70),(-30,-130),(75,-95),(135,-5),(65,95),(-55,115),(-135,35),(0,0)]
        lit = False
        for ax, ay in pts:
            if (dx-ax)**2 + (dy-ay)**2 < 17**2:
                lit = True
        for i, j in [(0,1),(1,2),(2,3),(3,4),(4,5),(5,6),(6,0),(0,7),(1,7),(2,7),(3,7),(4,7),(5,7),(6,7)]:
            x1,y1=pts[i]; x2,y2=pts[j]
            vx,vy=x2-x1,y2-y1
            t=max(0,min(1,((dx-x1)*vx+(dy-y1)*vy)/(vx*vx+vy*vy)))
            px,py=x1+t*vx,y1+t*vy
            if (dx-px)**2+(dy-py)**2 < 2.6**2:
                lit = True
        if lit:
            r,g,b = 70,205,255
        row.extend((r,g,b))
    rows.append(bytes(row))
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data) & 0xffffffff)
raw = b"".join(rows)
png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">2I5B", W, H, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw, 9))
png += chunk(b"IEND", b"")
output = sys.argv[1] if len(sys.argv) > 1 else "sce_sys/icon0.png"\nimport os\nos.makedirs(os.path.dirname(output) or ".", exist_ok=True)\nopen(output, "wb").write(png)

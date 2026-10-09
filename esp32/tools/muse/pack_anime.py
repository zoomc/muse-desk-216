"""Pack equal 3x3 atlases into a separate, versioned indexed RGB565 art partition.

Usage: pack_anime.py OUTPUT.bin SHEET.png [SECOND_SHEET.png ...]
Requires Pillow. The desk artwork and packed partition are in assets/muse-desk/.
"""
from pathlib import Path
import sys,struct,zlib
from PIL import Image
SIZE=320
output=Path(sys.argv[1]); frames=[]
for name in sys.argv[2:]:
    sheet=Image.open(name).convert('RGB')
    for index in range(9):
        col,row=index%3,index//3
        box=(round(col*sheet.width/3),round(row*sheet.height/3),round((col+1)*sheet.width/3),round((row+1)*sheet.height/3))
        frames.append(sheet.crop(box).resize((SIZE,SIZE),Image.Resampling.LANCZOS))
assert 1 <= len(frames) <= 64
mosaic=Image.new('RGB',(SIZE,SIZE*len(frames)))
for i,frame in enumerate(frames): mosaic.paste(frame,(0,i*SIZE))
quantized=mosaic.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
palette=quantized.getpalette()
colors=[]
for i in range(256):
    r,g,b=palette[i*3:i*3+3]
    colors.append((r>>3)<<11|(g>>2)<<5|(b>>3))
payload=struct.pack('<256H',*colors)+quantized.tobytes()
header=struct.pack('<4s6I',b'MART',1,SIZE,SIZE,len(frames),len(payload)+28,zlib.crc32(payload))
output.parent.mkdir(parents=True,exist_ok=True)
output.write_bytes(header+payload)
print(f'{len(frames)} native {SIZE}x{SIZE} poses, {len(payload)+28} bytes.')

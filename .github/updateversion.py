import re
import sys

if len(sys.argv) < 2:
    print("Usage: python3 updateversion.py <version>")
    sys.exit(1)

tag = sys.argv[1]

content_new = ''
with open ('./src/PixelIt.ino', 'r' ) as f:
    content = f.read()
    content_new = re.sub('(#define\s+VERSION\s+\")(.*)(\")', r'\g<1>'+ tag + r'\g<3>', content, flags = re.M)
    f.close()

if content_new != "":
    with open ('./src/PixelIt.ino', 'w' ) as f:
        f.write(content_new)
        f.close()

"""List SPW pointers with their containing DAT directory path."""
import argparse
import struct
from pathlib import Path

import inspect_zone_collision as dat


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dat")
    parser.add_argument("--context", default="")
    args = parser.parse_args()
    stack = []
    for index, chunk in enumerate(dat.parse_chunks(Path(args.dat).read_bytes())):
        if chunk["type"] == 1:
            stack.append(chunk["name"])
        elif chunk["type"] == 0:
            if stack:
                stack.pop()
        elif chunk["type"] == 0x3D and len(chunk["payload"]) >= 12:
            sound_id = struct.unpack_from("<I", chunk["payload"], 8)[0]
            identity = "/".join(stack) + "/" + chunk["name"]
            if not args.context or args.context.lower() in identity.lower():
                print(index, "/".join(stack), repr(chunk["name"]), sound_id,
                      f"se/se{sound_id // 1000:03d}/se{sound_id:06d}.spw")
        elif args.context and args.context.lower() in (
                "/".join(stack) + "/" + chunk["name"]).lower():
            print(index, "/".join(stack), repr(chunk["name"]),
                  f"type=0x{chunk['type']:02x}", f"size={chunk['size']}")


if __name__ == "__main__":
    main()

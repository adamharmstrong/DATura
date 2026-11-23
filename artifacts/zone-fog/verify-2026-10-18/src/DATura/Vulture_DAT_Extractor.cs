
// author: vulture
// license: public domain
//
// usage: dat [filename.dat]

#define WRITE_CHUNKS_TO_DISK
#define WRITE_COLLISION_MESH_TO_DISK
// other things comment/uncomment as desired whatever


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using System.IO;
using System.IO.Compression;
using System.Diagnostics;
using System.Threading;


namespace FFXI
{
    public class CHUNK
    {
        public string FourCC;
        public uint Length;
        public uint Type;
        public byte[] data;
        public int Number;

        public CHUNK(Stream s)
        {
            BinaryReader br = new BinaryReader(s);

            byte[] fcc = br.ReadBytes(4);
            int fcclen = 0;
            while (fcclen < fcc.Length && fcc[fcclen] != 0) fcclen++;
            FourCC = Encoding.ASCII.GetString(fcc, 0, fcclen);
            Length = br.ReadUInt32() & 0xfffffff;
            Type = Length & 0x7f;
            Length = ((Length >> 7) << 4) - 0x10;
            br.ReadUInt32();
            br.ReadUInt32();

            data = br.ReadBytes((int)Length);
        }

        public static void Decrypt05(byte[] data)
        {
            int len = BitConverter.ToInt32(data, 0) & 0xffffff;
            int key1 = data[5] ^ 0xf0;
            int key2 = KEY_TABLE1[key1];
            int counter = 0;

            for (int pos = 8; pos < len; pos++)
            {
                int x = (key2 & 0xff) * 0x0101;
                key2 += ++counter;

                data[pos] ^= (byte)(x >> (key2 & 7)); // I guess these together are supposed to be some hacky rotate?
                key2 += ++counter;
            }
        }

        public static void DecryptFFFF(byte[] data)
        {
            int len = BitConverter.ToInt32(data, 0) & 0xffffff;
            int key1 = data[5] ^ 0xf0;
            int key2 = KEY_TABLE1[key1];
            int len2 = ((len - 8) & ~0xf) >> 1;

            int offset1 = 8;
            int offset2 = offset1 + len2;

            byte[] tmp = new byte[8];

            for (int i = 0; i < len2; i += 8)
            {
                if (((key2) & 1) == 1)
                {
                    Array.Copy(data, offset1, tmp, 0, 8);
                    Array.Copy(data, offset2, data, offset1, 8);
                    Array.Copy(tmp, 0, data, offset2, 8);
                }
                key1 += 9;
                key2 += key1;   // seems like this would just swap, swap, notswap, notswap, repeat; with first swap position determined by key2 initial value
                offset1 += 8;
                offset2 += 8;
            }
        }

        public static void Decrypt1B(byte[] data)
        {
            int len = BitConverter.ToInt32(data, 0) & 0xffffff;
            int key = KEY_TABLE1[data[7]^0xff];
            int key_counter = 0;

            for (int pos = 8; pos < len; )
            {
                int xor_length = ((key >> 4) & 7) + 0x10;

                if ((key & 1) == 1 && (pos + xor_length < len))
                {
                    for (int i = 0; i < xor_length; i++)
                    {
                        data[pos + i] ^= 0xff;
                    }
                }

                key += ++key_counter;
                pos += xor_length;
            }

            int node_count = BitConverter.ToInt32(data, 4) & 0xffffff;

            for (int i = 0; i < node_count; i++)
            {
                for (int j = 0; j < 0x10; j++)
                {
                    data[0x20 + i * 0x64 + j] ^= 0x55;    // each node is 0x64 bytes?
                }
            }
        }

        public static void Decrypt(byte[] data)
        {
            if (data.Length < 8) return;

            if (data[3] == 0x05) Decrypt05(data);
            if (data[3] == 0x1b) Decrypt1B(data);
            if (BitConverter.ToUInt16(data,6) == 0xffff) DecryptFFFF(data);
        }




        private static byte[] KEY_TABLE1 =
        {
			0xE2, 0xE5, 0x06, 0xA9, 0xED, 0x26, 0xF4, 0x42,
			0x15, 0xF4, 0x81, 0x7F, 0xDE, 0x9A, 0xDE, 0xD0,
			0x1A, 0x98, 0x20, 0x91, 0x39, 0x49, 0x48, 0xA4,
			0x0A, 0x9F, 0x40, 0x69, 0xEC, 0xBD, 0x81, 0x81,
			0x8D, 0xAD, 0x10, 0xB8, 0xC1, 0x88, 0x15, 0x05,
			0x11, 0xB1, 0xAA, 0xF0, 0x0F, 0x1E, 0x34, 0xE6,
			0x81, 0xAA, 0xCD, 0xAC, 0x02, 0x84, 0x33, 0x0A,
			0x19, 0x38, 0x9E, 0xE6, 0x73, 0x4A, 0x11, 0x5D,
			0xBF, 0x85, 0x77, 0x08, 0xCD, 0xD9, 0x96, 0x0D,
			0x79, 0x78, 0xCC, 0x35, 0x06, 0x8E, 0xF9, 0xFE,
			0x66, 0xB9, 0x21, 0x03, 0x20, 0x29, 0x1E, 0x27,
			0xCA, 0x86, 0x82, 0xE6, 0x45, 0x07, 0xDD, 0xA9,
			0xB6, 0xD5, 0xA2, 0x03, 0xEC, 0xAD, 0x62, 0x45,
			0x2D, 0xCE, 0x79, 0xBD, 0x8F, 0x2D, 0x10, 0x18,
			0xE6, 0x0A, 0x6F, 0xAA, 0x6F, 0x46, 0x84, 0x32,
			0x9F, 0x29, 0x2C, 0xC2, 0xF0, 0xEB, 0x18, 0x6F,
			0xF2, 0x3A, 0xDC, 0xEA, 0x7B, 0x0C, 0x81, 0x2D,
			0xCC, 0xEB, 0xA1, 0x51, 0x77, 0x2C, 0xFB, 0x49,
			0xE8, 0x90, 0xF7, 0x90, 0xCE, 0x5C, 0x01, 0xF3,
			0x5C, 0xF4, 0x41, 0xAB, 0x04, 0xE7, 0x16, 0xCC,
			0x3A, 0x05, 0x54, 0x55, 0xDC, 0xED, 0xA4, 0xD6,
			0xBF, 0x3F, 0x9E, 0x08, 0x93, 0xB5, 0x63, 0x38,
			0x90, 0xF7, 0x5A, 0xF0, 0xA2, 0x5F, 0x56, 0xC8,
			0x08, 0x70, 0xCB, 0x24, 0x16, 0xDD, 0xD2, 0x74,
			0x95, 0x3A, 0x1A, 0x2A, 0x74, 0xC4, 0x9D, 0xEB,
			0xAF, 0x69, 0xAA, 0x51, 0x39, 0x65, 0x94, 0xA2,
			0x4B, 0x1F, 0x1A, 0x60, 0x52, 0x39, 0xE8, 0x23,
			0xEE, 0x58, 0x39, 0x06, 0x3D, 0x22, 0x6A, 0x2D,
			0xD2, 0x91, 0x25, 0xA5, 0x2E, 0x71, 0x62, 0xA5,
			0x0B, 0xC1, 0xE5, 0x6E, 0x43, 0x49, 0x7C, 0x58,
			0x46, 0x19, 0x9F, 0x45, 0x49, 0xC6, 0x40, 0x09,
			0xA2, 0x99, 0x5B, 0x7B, 0x98, 0x7F, 0xA0, 0xD0,
        };

        private static byte[] KEY_TABLE2 =
        {
			0xB8, 0xC5, 0xF7, 0x84, 0xE4, 0x5A, 0x23, 0x7B,
			0xC8, 0x90, 0x1D, 0xF6, 0x5D, 0x09, 0x51, 0xC1,
			0x07, 0x24, 0xEF, 0x5B, 0x1D, 0x73, 0x90, 0x08,
			0xA5, 0x70, 0x1C, 0x22, 0x5F, 0x6B, 0xEB, 0xB0,
			0x06, 0xC7, 0x2A, 0x3A, 0xD2, 0x66, 0x81, 0xDB,
			0x41, 0x62, 0xF2, 0x97, 0x17, 0xFE, 0x05, 0xEF,
			0xA3, 0xDC, 0x22, 0xB3, 0x45, 0x70, 0x3E, 0x18,
			0x2D, 0xB4, 0xBA, 0x0A, 0x65, 0x1D, 0x87, 0xC3,
			0x12, 0xCE, 0x8F, 0x9D, 0xF7, 0x0D, 0x50, 0x24,
			0x3A, 0xF3, 0xCA, 0x70, 0x6B, 0x67, 0x9C, 0xB2,
			0xC2, 0x4D, 0x6A, 0x0C, 0xA8, 0xFA, 0x81, 0xA6,
			0x79, 0xEB, 0xBE, 0xFE, 0x89, 0xB7, 0xAC, 0x7F,
			0x65, 0x43, 0xEC, 0x56, 0x5B, 0x35, 0xDA, 0x81,
			0x3C, 0xAB, 0x6D, 0x28, 0x60, 0x2C, 0x5F, 0x31,
			0xEB, 0xDF, 0x8E, 0x0F, 0x4F, 0xFA, 0xA3, 0xDA,
			0x12, 0x7E, 0xF1, 0xA5, 0xD2, 0x22, 0xA0, 0x0C,
			0x86, 0x8C, 0x0A, 0x0C, 0x06, 0xC7, 0x65, 0x18,
			0xCE, 0xF2, 0xA3, 0x68, 0xFE, 0x35, 0x96, 0x95,
			0xA6, 0xFA, 0x58, 0x63, 0x41, 0x59, 0xEA, 0xDD,
			0x7F, 0xD3, 0x1B, 0xA8, 0x48, 0x44, 0xAB, 0x91,
			0xFD, 0x13, 0xB1, 0x68, 0x01, 0xAC, 0x3A, 0x11,
			0x78, 0x30, 0x33, 0xD8, 0x4E, 0x6A, 0x89, 0x05,
			0x7B, 0x06, 0x8E, 0xB0, 0x86, 0xFD, 0x9F, 0xD7,
			0x48, 0x54, 0x04, 0xAE, 0xF3, 0x06, 0x17, 0x36,
			0x53, 0x3F, 0xA8, 0x11, 0x53, 0xCA, 0xA1, 0x95,
			0xC2, 0xCD, 0xE6, 0x1F, 0x57, 0xB4, 0x7F, 0xAA,
			0xF3, 0x6B, 0xF9, 0xA0, 0x27, 0xD0, 0x09, 0xEF,
			0xF6, 0x68, 0x73, 0x60, 0xDC, 0x50, 0x2A, 0x25,
			0x0F, 0x77, 0xB9, 0xB0, 0x04, 0x0B, 0xE1, 0xCC,
			0x35, 0x31, 0x84, 0xE6, 0x22, 0xF9, 0xC2, 0xAB,
			0x95, 0x91, 0x61, 0xD9, 0x2B, 0xB9, 0x72, 0x4E,
			0x10, 0x76, 0x31, 0x66, 0x0A, 0x0B, 0x2E, 0x83,
        };
 
    }

    public class DAT
    {
        public List<CHUNK> Chunks = new List<CHUNK>();

        public DAT(Stream s)
        {
            BinaryReader br = new BinaryReader(s);
            while (s.Position < s.Length)
            {
                long offset = s.Position;
                CHUNK c = new CHUNK(s);
                uint d1 = (c.data.Length >= 4) ? BitConverter.ToUInt32(c.data, 0) : 0;
                uint d2 = (c.data.Length >= 8) ? BitConverter.ToUInt32(c.data, 4) : 0;
                //Console.WriteLine("chunk: {0,8:x8} : {1}:{2,2:x2} {3,8:x8} bytes / {4,8:x8} {5,8:x8}", offset, c.FourCC, c.Type, c.Length, d1, d2);

                //File.WriteAllBytes(string.Format("{0,4:x4}{1}.enc", Chunks.Count, c.FourCC), c.data);
                CHUNK.Decrypt(c.data);
                string filename = string.Format("{0,4:x4}-{1}-{2,2:x2}.dec", Chunks.Count, c.FourCC, c.Type);
#if WRITE_CHUNKS_TO_DISK
                if (!File.Exists(filename)) File.WriteAllBytes(filename, c.data);
#endif
                c.Number = Chunks.Count;

                Chunks.Add(c);
            }
        }


        public static int ParseMapIDStruct(byte[] data, int pos)
        {
            float[] f = new float[0x29];

            for (int i = 0; i < f.Length; i++) f[i] = BitConverter.ToSingle(data, pos + i * 4);
            uint mapidencoded = BitConverter.ToUInt32(data, pos + 0x29 * 4);
            int objvisoffset = BitConverter.ToInt32(data, pos + 0x2a * 4);
            int objviscount = BitConverter.ToInt32(data, pos + 0x2b * 4);
            // 0x2c = 0
            float x = BitConverter.ToSingle(data, pos + 0x2d * 4);
            float y = BitConverter.ToSingle(data, pos + 0x2e * 4);
            // 0x2f = 0

            uint mapid = ((mapidencoded >> 3) & 0x7) | (((mapidencoded >> 26) & 0x3) << 3);

            Console.Write("{0,7:0.00},{1,7:0.00} | {2,2:x2} |", x, y, mapid);
            for (int i = 0; i < f.Length; i++) Console.Write(" {0,7:0.00}", f[i]);
            Console.WriteLine();

            return (int)mapid;
        }

        public class BBT
        {
            public float x1 = 9999, y1 = 9999, z1 = 9999;
            public float x2 = -9999, y2 = -9999, z2 = -9999;
            public List<int> matches = new List<int>();
            public List<BBT> children = new List<BBT>();
            public List<int> matchids = new List<int>();

            public BBT(float[] f)
            {
                for (int i = 0; i < 4; i++)
                {
                    // not exactly the most efficient way to do this
                    x1 = Math.Min(x1, f[i * 4 + 0]);
                    x2 = Math.Max(x2, f[i * 4 + 0]);
                    y1 = Math.Min(y1, f[i * 4 + 1]);
                    y2 = Math.Max(y2, f[i * 4 + 1]);
                    z1 = Math.Min(z1, f[i * 4 + 2]);
                    z2 = Math.Max(z2, f[i * 4 + 2]);
                }
            }

            public BBT find(float x, float y, float z)
            {
                foreach (BBT child in children)
                {
                    if (x >= child.x1 && x <= child.x2 && y >= child.y1 && y <= child.y2 && z >= child.z1 && z <= child.z2)
                    {
                        return child.find(x, y, z);
                    }
                }

                if (x >= x1 && x <= x2 && y >= y1 && y <= y2 && z >= z1 && z <= z2)
                {
                    return this;
                }
                else
                {
                    return null;
                }
            }
        }


        public static Dictionary<int, int> ByteCover = new Dictionary<int, int>();
        public static Dictionary<int, string> ByteCoverName = new Dictionary<int, string>();

        public static void AddByteCover(string name, int pos, int length)
        {
            ByteCover[pos] = length;
            ByteCoverName[pos] = name;
        }

        public static void PrintByteCover()
        {
            string lastname = "";
            int lastpos = 0;
            int lastlen = 0;

            foreach (var kvp in ByteCover.OrderBy(x => x.Key))
            {
                int pos = kvp.Key;
                int len = kvp.Value;
                string name = ByteCoverName[pos];

                if (name == lastname && pos == lastpos + lastlen)
                {
                    // combine
                    lastlen += len;
                }
                else
                {
                    if (lastlen > 0) Console.WriteLine("{0,20} : {1}-{2} ({3})", lastname, lastpos.ToString("x8"), (lastpos + lastlen).ToString("x8"), lastlen.ToString("x8"));

                    if (lastpos + lastlen != pos) Console.WriteLine("{0,20} : {1}-{2} ({3})", "[[skipped]]", (lastpos + lastlen).ToString("x8"), pos.ToString("x8"), (pos - (lastpos + lastlen)).ToString("x8"));

                    lastname = name;
                    lastpos = pos;
                    lastlen = len;
                }
            }

            if (lastlen > 0) Console.WriteLine("{0,20} : {1}-{2} ({3})", lastname, lastpos.ToString("x8"), (lastpos + lastlen).ToString("x8"), lastlen.ToString("x8"));
        }


        public static BBT ParseQuadTree(byte[] data, int pos, int level)
        {
            float[] BB = new float[8 * 3];
            int[] children = new int[4];

            for (int i = 0; i < 8 * 3; i++) BB[i] = BitConverter.ToSingle(data, pos + i * 4);
            int vislistoffset = BitConverter.ToInt32(data, pos + 8 * 3 * 4);
            int vislistcount = BitConverter.ToInt32(data, pos + 8 * 3 * 4 + 4);
            for (int i = 0; i < 4; i++) children[i] = BitConverter.ToInt32(data, pos + 8 * 3 * 4 + 4 + 4 + i * 4);

            AddByteCover("QuadTree", pos, 8 * 3 * 4 + 4 + 4 + 4 * 4 + 8);

            Console.Write("{0}", new string(' ', level * 8));
            for (int i = 0; i < 8 * 3; i++)
            {
                Console.Write(" {0,7:0.00}", BB[i]);
            }

            BBT b = new BBT(BB);

            HashSet<int> visset = new HashSet<int>();

            if (vislistcount > 0)
            {
                Console.Write("    vis:");

                AddByteCover("Vis", vislistoffset, vislistcount * 4);

                for (int i = 0; i < vislistcount; i++)
                {
                    int node = BitConverter.ToInt32(data, vislistoffset + i * 4);
                    Console.Write(" {0,4:x4}", node);
                    b.matchids.Add(node);

                    int mapid = vismapid[node];
                    if (!visset.Contains(mapid)) visset.Add(mapid);
                }

                Console.Write(" | all mapids:");
                foreach (int mapid in visset)
                {
                    Console.Write(" {0}", mapid);
                    b.matches.Add(mapid);
                }
            }
            Console.WriteLine();

            for (int i = 0; i < 4; i++)
            {
                if (children[i] != 0)
                {
                    BBT child = ParseQuadTree(data, children[i], level + 1);
                    b.children.Add(child);
                }
            }

            return b;
        }

        public static Dictionary<int, int> vismapid = new Dictionary<int, int>();

        public static void Parse1C(CHUNK c)
        {
            // map

            int meshoffset = BitConverter.ToInt32(c.data, 8);           // stores info about the collision mesh
            int gridwidth = 10 * c.data[0x0c];  // stored div 10
            int gridheight = 10 * c.data[0x0d];
            int bucketwidth = c.data[0x0e];     // almost always 40?
            int bucketheight = c.data[0x0f];    // stored mul 10; so 40 = 4 yalms; probably baked in so gridwidth*bucketwidth = mapwidth
            int quadtreeoffset = BitConverter.ToInt32(c.data, 0x10);
            int objectsoffset = 0x20;
            int objectsendoffset = BitConverter.ToInt32(c.data, 0x14);
            int objectscount = (objectsendoffset - objectsoffset) / 0x64;
            int shortnameoffset = BitConverter.ToInt32(c.data, 0x18);
            int shortnamescount = (meshoffset - shortnameoffset) / 0x4c;
            AddByteCover("header", 0, 0x20);

            // collision mesh
            // - use the grid for fast location-based queries, or
            // - use the list directly if you want to enumerate all the meshes (grid is also fine for this, just skip over the null pointers)
            // - dont use the mesh data directly because it stores grid-local coordinates and also lacks transformations
            int meshmodelcount = BitConverter.ToInt32(c.data, meshoffset + 0x00);
            int meshmodeldata = BitConverter.ToInt32(c.data, meshoffset + 0x04);                // raw mesh data, don't use this, it is local object data and doesn't have transformations applied
            int meshgridbucketlistscount = BitConverter.ToInt32(c.data, meshoffset + 0x08);
            int meshgridbucketlists = BitConverter.ToInt32(c.data, meshoffset + 0x0c);          // each bucket has a list of {meshdata,transformation} and the list is zero terminated
            int gridoffset = BitConverter.ToInt32(c.data, meshoffset + 0x10);                   // whole map is divided into a grid; each bucket points to its list entry

            Console.WriteLine("mesh: {0} {1} {2} {3} {4}", meshmodelcount.ToString("x8"), meshmodeldata.ToString("x8"), meshgridbucketlistscount.ToString("x8"), meshgridbucketlists.ToString("x8"), gridoffset.ToString("x8"));

            int mapidlistoffset = BitConverter.ToInt32(c.data, meshoffset + 0x14);
            int mapidlistcount = BitConverter.ToInt32(c.data, meshoffset + 0x18);
            AddByteCover("meshheader", meshoffset, 0x20);

            Console.WriteLine("meshes:");
            int j = meshmodeldata;
            for (int i = 0; i < meshmodelcount; i++)
            {
                //Console.Write("{0,4:x4} |", i);
                int retj = ParseMesh(c.data, j);
                AddByteCover("meshes", j, retj - j);
                j = retj;
            }

            // this loop populates global AllVertices AllNormals AllTriangles (sorry for statics; if you plan on using this for more than one map at a time, rewrite to be less staticy)
            Console.WriteLine("grid:");
            for (int y = 0; y < gridheight; y++)
            {
                for (int x = 0; x < gridwidth; x++)
                {
                    int offs = (y * gridwidth + x) * 4;
                    int entryoffs = BitConverter.ToInt32(c.data, gridoffset + offs);
                    if (entryoffs != 0) ParseGridEntry(c.data, entryoffs, x, y);
                    AddByteCover("grid", gridoffset + offs, 4);
                }
            }

#if WRITE_COLLISION_MESH_TO_DISK
            // write out the collision mesh in some format
            using (var sw = new StreamWriter("out.obj"))
            {
                foreach (var v in AllVertices) sw.WriteLine("v {0} {1} {2}", v.x, v.y, v.z);
                foreach (var n in AllNormals) sw.WriteLine("vn {0} {1} {2}", n.nx, n.ny, n.nz);

                foreach (var t in AllTriangles) sw.WriteLine("f {0}//{1} {2}//{3} {4}//{5}",
                    1 + t.iv0, 1 + t.in0,
                    1 + t.iv1, 1 + t.in0,
                    1 + t.iv2, 1 + t.in0
                    );
            }
#endif

            Console.WriteLine("shortnames:");
            for (int i = 0; i < shortnamescount; i++)
            {
                string name = Encoding.ASCII.GetString(c.data, shortnameoffset + i * 0x4c, 0x20);
                //Console.Write("{0,4:x4} | {1}", i, name);
                AddByteCover("shortnames", shortnameoffset + i * 0x4c, 0x4c);
            }
            Console.WriteLine();


            Console.WriteLine("objects:");
            for (int i = 0; i < objectscount; i++)
            {
                Console.Write("{0,4:x4} |", i);
                ParseObject(c.data, objectsoffset + i * 0x64);
                AddByteCover("objects", objectsoffset + i * 0x64, 0x64);
            }


            Console.WriteLine("vislist:");
            for (int i = 0; i < mapidlistcount; i++)
            {
                Console.Write("{0,4:x4} |", i);
                int mapid = ParseMapIDStruct(c.data, mapidlistoffset + 0xc0 * i);
                vismapid.Add(i, mapid);
                AddByteCover("vislist", mapidlistoffset + 0xc0 * i, 0xc0);
            }

            // quadtree does multiple things:
            // - given a position, traverse down to some bucket
            // - each bucket tells you what things are visible, which improves speed of game by limiting objects/polygons drawn
            // - also tells you the 2D sub map id for a given 3D point (to pull up the proper 2D image when you open up /map)
            // quadtree volumes have some transformations that I haven't bothered to apply
            // quadtree stores entire vertical column (i.e. it is not really an octree)
            Console.WriteLine("quadtree:");
            BBT root = ParseQuadTree(c.data, quadtreeoffset, 1);

#if false
            // query position to mapid(s)
            BBT found = root.find(-550, 60, -70);
            foreach (int node in found.matchids) Console.WriteLine("{0}", node);
            foreach (int mapid in found.matches) Console.WriteLine("{0}", mapid);
#endif

            AddByteCover("EOF", c.data.Length, 1);

            PrintByteCover();
        }

        private static void ParseGridEntry(byte[] data, int entryoffs, int x, int y)
        {
            var entries = new List<int>();
            while (true)
            {
                int c = BitConverter.ToInt32(data, entryoffs);
                AddByteCover("gridlist", entryoffs, 4);
                if (c == 0) break;
                entries.Add(c);
                entryoffs += 4;
            }

            uint pos = (uint)entries[0];
            int xx = (int)((pos >> 14) & 0x1ff);
            int yy = (int)((pos >> 23) & 0x1ff);
            int flags = (int)(pos & 0x3fff);

            //Console.Write("{0} [{1}] : {2},{3} :", entryoffs.ToString("x8"), flags.ToString("x4"), x.ToString("x4"), y.ToString("x4"));
            for (int i = 1; i < entries.Count; i += 2)
            {
                //Console.Write(" {0}", entries[i].ToString("x8"));
                int visentryoffset = entries[i + 0];        // points to a raw transformation matrix[4][4]
                int geometryoffset = entries[i + 1];        // mesh data

                ParseGridMesh(data, x, y, visentryoffset, geometryoffset);
            }
            //Console.WriteLine();
        }

        public struct vertex
        {
            public float x, y, z;
        }

        public struct normal
        {
            public float nx, ny, nz;
        }

        public struct triangle
        {
            public int iv0, iv1, iv2;
            public int in0;
        }


        public static List<vertex> AllVertices = new List<vertex>();
        public static List<normal> AllNormals = new List<normal>();
        public static List<triangle> AllTriangles = new List<triangle>();


        private static void ParseGridMesh(byte[] data, int _x, int _y, int visentryoffset, int geometryoffset)
        {
            var M = new float[16];
            for (int i = 0; i < 16; i++) M[i] = BitConverter.ToSingle(data, visentryoffset + i * 4);

            //float tx = M[12];
            //float ty = M[13];
            //float tz = M[14];

            int vertices = BitConverter.ToInt32(data, geometryoffset + 0x00);
            int normals = BitConverter.ToInt32(data, geometryoffset + 0x04);
            int tris = BitConverter.ToInt32(data, geometryoffset + 0x08);
            int tricount = BitConverter.ToInt16(data, geometryoffset + 0x0c);
            int flags = BitConverter.ToInt16(data, geometryoffset + 0x0e);              // 0 = block pathing and line of sight; 1 = only blocks pathing (not line of sight)

            bool doesntBlockLineOfSight = ((flags & 1) != 0);
            //if (doesntBlockLineOfSight) return;

            int numvert = (normals - vertices) / 12;
            int numnorm = (tris - normals) / 12;

            // append new verts/norms/tris to end of current set
            int basevert = AllVertices.Count;
            int basenorm = AllNormals.Count;
            int basetri = AllTriangles.Count;

            for (int i = 0; i < numvert; i++)
            {
                float x = BitConverter.ToSingle(data, vertices + (i * 3 + 0) * 4);
                float y = BitConverter.ToSingle(data, vertices + (i * 3 + 1) * 4);
                float z = BitConverter.ToSingle(data, vertices + (i * 3 + 2) * 4);
                float w = 1.0f;

                //if(Math.Abs((M[0] * x + M[4] * y + M[8] * z + M[12] * w) - (-135.1522)) < 0.001) System.Diagnostics.Debugger.Break();

                AllVertices.Add(new vertex()
                {
                    x = M[0] * x + M[4] * y + M[8] * z + M[12] * w,
                    y = -(M[1] * x + M[5] * y + M[9] * z + M[13] * w),
                    z = M[2] * x + M[6] * y + M[10] * z + M[14] * w,
                });
            }

            for (int i = 0; i < numnorm; i++)
            {
                AllNormals.Add(new normal()
                {
                    nx = BitConverter.ToSingle(data, normals + (i * 3 + 0) * 4),
                    ny = -BitConverter.ToSingle(data, normals + (i * 3 + 1) * 4),
                    nz = BitConverter.ToSingle(data, normals + (i * 3 + 2) * 4),
                });
            }

            for (int i = 0; i < tricount; i++)
            {
                //if (basevert + (BitConverter.ToUInt16(data, tris + (i * 4 + 1) * 2) & 0x3fff) == 14783) System.Diagnostics.Debugger.Break();

                AllTriangles.Add(new triangle()
                {
                    iv0 = basevert + (BitConverter.ToUInt16(data, tris + (i * 4 + 0) * 2) & 0x3fff),
                    iv1 = basevert + (BitConverter.ToUInt16(data, tris + (i * 4 + 1) * 2) & 0x3fff),
                    iv2 = basevert + (BitConverter.ToUInt16(data, tris + (i * 4 + 2) * 2) & 0x3fff),
                    in0 = basenorm + (BitConverter.ToUInt16(data, tris + (i * 4 + 3) * 2) & 0x3fff),
                });
            }
        }

        private static int ParseMesh(byte[] data, int pos)
        {
            int vertices = BitConverter.ToInt32(data, pos + 0x00);
            int normals = BitConverter.ToInt32(data, pos + 0x04);
            int tris = BitConverter.ToInt32(data, pos + 0x08);
            int tricount = BitConverter.ToInt16(data, pos + 0x0c);
            int unk2 = BitConverter.ToInt16(data, pos + 0x0e);
            //Console.WriteLine(" {0} {1} {2} {3} {4}", vertices.ToString("x8"), normals.ToString("x8"), tris.ToString("x8"), tricount.ToString("x4"), unk2.ToString("x4"));
            return tris + tricount * 2 * 4;
        }

        private static void ParseObject(byte[] data, int pos)
        {
            string modelname = Encoding.ASCII.GetString(data, pos, 0x10);
            float[] position = new float[3]
            {
                BitConverter.ToSingle(data, pos + 0x10 + 0),
                BitConverter.ToSingle(data, pos + 0x10 + 4),
                BitConverter.ToSingle(data, pos + 0x10 + 8),
            };
            float[] rotation = new float[3]
            {
                BitConverter.ToSingle(data, pos + 0x10 + 0x0c),
                BitConverter.ToSingle(data, pos + 0x10 + 0x10),
                BitConverter.ToSingle(data, pos + 0x10 + 0x14),
            };
            float[] scale = new float[3]
            {
                BitConverter.ToSingle(data, pos + 0x10 + 0x18),
                BitConverter.ToSingle(data, pos + 0x10 + 0x1c),
                BitConverter.ToSingle(data, pos + 0x10 + 0x20),
            };
            string ingamename = Encoding.ASCII.GetString(data, pos + 0x34, 4);
            if (ingamename[0] == 0) ingamename = "    ";
            // 0x38, 0x3c, 0x40 -- 10.0, 100.0, 1000.0
            // 0x44 -- flags? bigendian 1?
            // 0x48 -- offset to list of indices
            int listoffset = BitConverter.ToInt32(data, pos + 0x48);
            // 0x4c, 0x50 -- 0?
            int[] m = new int[4]
            {
                BitConverter.ToInt32(data, pos + 0x54 + 0),
                BitConverter.ToInt32(data, pos + 0x54 + 4),
                BitConverter.ToInt32(data, pos + 0x54 + 8),
                BitConverter.ToInt32(data, pos + 0x54 + 0xc),
            };
            // 64 -- end

            Console.Write("{0} @ {1,6} {2,6} {3,6} | {4} {5} {6} | {7,5} {8,5} {9,5} | {10} | {11} | {12} {13} {14} {15}", modelname,
                position[0].ToString("N1"), position[1].ToString("N1"), position[2].ToString("N1"),
                rotation[0].ToString("N2"), rotation[1].ToString("N2"), rotation[2].ToString("N2"),
                scale[0].ToString("N2"), scale[1].ToString("N2"), scale[2].ToString("N2"),
                ingamename,
                listoffset.ToString("x8"),
                m[0].ToString("x4"), m[1].ToString("x4"), m[2], m[3]);

            if (listoffset > 0) ParseObjectListOffset(data, listoffset);
            else Console.WriteLine();
        }

        private static void ParseObjectListOffset(byte[] data, int pos)
        {
            Console.Write(" |");
            // # of entries => n
            // n times int index (mapidlistcount)
            int length = BitConverter.ToInt32(data, pos);
            for (int i = 0; i < length; i++)
            {
                if (i >= 5) { Console.Write(" ..."); break; }
                int j = BitConverter.ToInt32(data, pos + 4 + i * 4);
                Console.Write(" {0}", j.ToString("x4"));
            }
            Console.WriteLine();

            AddByteCover("objectlist", pos, length * 4 + 4);
        }


        public static void Parse2E(CHUNK c)
        {
            int hcount = c.data[0x20];
            //if (maxcount != 0 && maxcount != 1) Console.WriteLine("{0,4:x4}", c.Number);
            if (hcount == 0) return; // whatever
            //if (c.data[0x60] != 'm') Console.WriteLine("{0,4:x4}", c.Number);
            // type at [0x60..0x67] is "warp    " or "model   " or "        "


            int numpoints = BitConverter.ToInt32(c.data, 0x70);
            for (int i = 0; i < numpoints; i++)
            {

            }
        }

        public static void Parse20(CHUNK c)
        {
            //Debugger.Break();
            var filename = string.Format("{0,4:x4}-{1}-{2,2:x2}.dec", c.Number, c.FourCC, c.Type);
            var filenamepng = string.Format("{0,4:x4}-{1}-{2,2:x2}.png", c.Number, c.FourCC, c.Type);
#if false
            var psi = new ProcessStartInfo("dxt3.exe", "\"" + filename + "\"");
            var p = Process.Start(psi);
            p.WaitForExit();
            if (!Directory.Exists("out")) Directory.CreateDirectory("out");
            if (File.Exists("out.png")) File.Move("out.png", Path.Combine("out", filenamepng));
#endif
        }

        public struct Subregion
        {
            public float x;
            public float y;
            public float z;
            public float rx;
            public float ry;
            public float rz;
            public float vx;
            public float vy;
            public float vz;
            public string name;
            public int zoneid;
            public int srcordst;
        }

        public static double rotx(float x, float y, float theta)
        {
            return x * Math.Cos(theta) - y * Math.Sin(theta);
        }

        public static double roty(float x, float y, float theta)
        {
            return x * Math.Sin(theta) + y * Math.Cos(theta);
        }

        public static void Parse36(CHUNK c)
        {
            // if .zoneid same as current zone, region is probably a "zone line"
            // - if srcordst is 0, the volume zones out
            // - if srcordst is 1, the volume is the random region where you zone in (but your rotation will come from zone out)

            var l = new Subregion();

            int count = BitConverter.ToInt32(c.data, 0x30);

            for (int i = 0; i < count; i++)
            {
                var o = new Subregion();

                o.x = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x00);
                o.y = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x04);
                o.z = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x08);
                o.rx = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x0c);
                o.ry = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x10);
                o.rz = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x14);
                o.vx = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x18);
                o.vy = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x1c);
                o.vz = BitConverter.ToSingle(c.data, 0x40 + i * 0x40 + 0x20);

                byte[] namebytes = new byte[8];
                Array.Copy(c.data, 0x40 + i * 0x40 + 0x24, namebytes, 0, 8);
                int len = 0;
                while (len < namebytes.Length && namebytes[len] != 0) len++;
                len = 8;
                o.name = Encoding.ASCII.GetString(namebytes, 0, len);

                o.zoneid = BitConverter.ToInt32(c.data, 0x40 + i * 0x40 + 0x2c);
                o.srcordst = BitConverter.ToInt32(c.data, 0x40 + i * 0x40 + 0x30);

#if true
                Console.WriteLine("{0,8} {1} {2} : {3,8},{4,8},{5,8} : {6,8},{7,8},{8,8} : {9,8},{10,8},{11,8}",
                    o.name, o.srcordst, o.zoneid.ToString("x4"),
                    o.x.ToString("N2"), o.y.ToString("N2"), o.z.ToString("N2"),
                    o.rx.ToString("N2"), o.ry.ToString("N2"), o.rz.ToString("N2"),
                    o.vx.ToString("N2"), o.vy.ToString("N2"), o.vz.ToString("N2")
                );
#endif

                if (true || o.srcordst == 0)
                {
                    float vx = o.vx / 2;
                    float vy = o.vy / 2;
                    float vz = o.vz / 2;
                    float ry = -o.ry;

                    Console.WriteLine("{0} {1} {2} {3} {4} {5} 0.5", o.x + rotx(-vx, vz, ry), o.z + roty(-vx, vz, ry), o.y, o.x + rotx(vx, vz, ry), o.z + roty(vx, vz, ry), o.y);
                    Console.WriteLine("{0} {1} {2} {3} {4} {5} 0.5", o.x + rotx(vx, vz, ry), o.z + roty(vx, vz, ry), o.y, o.x + rotx(vx, -vz, ry), o.z + roty(vx, -vz, ry), o.y);
                    Console.WriteLine("{0} {1} {2} {3} {4} {5} 0.5", o.x + rotx(vx, -vz, ry), o.z + roty(vx, -vz, ry), o.y, o.x + rotx(-vx, -vz, ry), o.z + roty(-vx, -vz, ry), o.y);
                    Console.WriteLine("{0} {1} {2} {3} {4} {5} 0.5", o.x + rotx(-vx, -vz, ry), o.z + roty(-vx, -vz, ry), o.y, o.x + rotx(-vx, vz, ry), o.z + roty(-vx, vz, ry), o.y);

                    //Console.WriteLine("{0} {1} {2} {3} {4} {5} 0.5", o.x + rotx(-vx, -vz, ry), o.z + roty(-vx, -vz, ry), o.y, o.x + rotx(vx, vz, ry), o.z + roty(vx, vz, ry), o.y);
                    //Console.WriteLine("{0} {1} {2} {3} {4} {5} 0.5", o.x + rotx(vx, -vz, ry), o.z + roty(vx, -vz, ry), o.y, o.x + rotx(-vx, -vz, ry), o.z + roty(-vx, -vz, ry), o.y);
                }

                l = o;
            }
        }




        public static void Main(string[] args)
        {
            DAT dat = new DAT(File.OpenRead(args[0]));

            foreach (CHUNK c in dat.Chunks)
            {
                if (false) { }
                else if (c.Type == 0x1c) Parse1C(c);
                //else if (c.Type == 0x36) Parse36(c);
                //else if (c.Type == 0x2e) Parse2E(c);
                //else if (c.Type == 0x20) Parse20(c);
            }
        }
    }
}

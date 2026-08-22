#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "helpdeco.h"

/* globals defined in helpdeco.c */
extern char HelpFileName[NAME_MAX];
extern char name[NAME_MAX];
extern char ext[_MAX_EXT];
extern BOOL mvp;
extern char oldtable[256];

/* functions defined in helpdeco.c (not declared in helpdeco.h) */
extern BOOL HelpDeCompile(FILE *HelpFile, char *dumpfile, int mode, char *exportname, long offset);

/*
 * Override getch() (imported from msvcrt by default) so the decompiler
 * never blocks waiting for interactive input while used as a library.
 * Always behaves as if Enter was pressed: error() continues and the
 * "Overwrite (Y/N/All/Quit)?" prompt defaults to Yes.
 */
int getch(void)
{
    return '\r';
}

/* initialize the hash coding table used to decode topic/context names */
static void init_hash_table(void)
{
    int i;

    memset(oldtable, 0, sizeof(oldtable));
    for (i = 0; i < 9; i++) oldtable['1' + i] = (char)(i + 1);
    oldtable['0'] = 10;
    oldtable['.'] = 12;
    oldtable['_'] = 13;
    for (i = 0; i < 26; i++) oldtable['A' + i] = oldtable['a' + i] = (char)(17 + i);
}

/*
 * Read the contents charset byte (first byte of |SYSTEM record 0x0B) used
 * to select the code page of the external .cnt file. Returns the byte value
 * or -1 if the help file has no usable record.
 */
int helpdeco_system_charset(const char *hlpPath)
{
    FILE *f;
    SYSTEMRECORD *rec;
    int charset = -1;

    if (!hlpPath)
        return -1;

    f = fopen(hlpPath, "rb");
    if (!f)
        return -1;

    for (rec = GetFirstSystemRecord(f); rec; rec = GetNextSystemRecord(rec))
    {
        if (rec->RecordType == 0x0B && rec->DataSize > 0)
        {
            charset = (unsigned char)rec->Data[0];
            break;
        }
    }

    fclose(f);
    return charset;
}

/*
 * Encode a readable context name ("_dx5_Legal_Information_legal") the same
 * way WinHelp/helpdeco stores it in the # footnotes of the decompiled RTF
 * (encoded hash, e.g. "IRR81I"). Used to match external .cnt entries to
 * the topics parsed from the .rtf, whose anchors hold the encoded form.
 *
 * Writes the NUL-terminated result to out (up to outsz bytes).
 * Returns 1 on success, 0 on failure.
 */
int helpdeco_context_id(const char *name, char *out, size_t outsz)
{
    /* hash encoding table, same values as the static table[] in helpdeco.c */
    static const signed char htable[256] =
    {
        '\x00', '\xD1', '\xD2', '\xD3', '\xD4', '\xD5', '\xD6', '\xD7', '\xD8', '\xD9', '\xDA', '\xDB', '\xDC', '\xDD', '\xDE', '\xDF',
        '\xE0', '\xE1', '\xE2', '\xE3', '\xE4', '\xE5', '\xE6', '\xE7', '\xE8', '\xE9', '\xEA', '\xEB', '\xEC', '\xED', '\xEE', '\xEF',
        '\xF0', '\x0B', '\xF2', '\xF3', '\xF4', '\xF5', '\xF6', '\xF7', '\xF8', '\xF9', '\xFA', '\xFB', '\xFC', '\xFD', '\x0C', '\xFF',
        '\x0A', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0F',
        '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
        '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27', '\x28', '\x29', '\x2A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0D',
        '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
        '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27', '\x28', '\x29', '\x2A', '\x2B', '\x2C', '\x2D', '\x2E', '\x2F',
        '\x50', '\x51', '\x52', '\x53', '\x54', '\x55', '\x56', '\x57', '\x58', '\x59', '\x5A', '\x5B', '\x5C', '\x5D', '\x5E', '\x5F',
        '\x60', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', '\x68', '\x69', '\x6A', '\x6B', '\x6C', '\x6D', '\x6E', '\x6F',
        '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', '\x78', '\x79', '\x7A', '\x7B', '\x7C', '\x7D', '\x7E', '\x7F',
        '\x80', '\x81', '\x82', '\x83', '\x0B', '\x85', '\x86', '\x87', '\x88', '\x89', '\x8A', '\x8B', '\x8C', '\x8D', '\x8E', '\x8F',
        '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97', '\x98', '\x99', '\x9A', '\x9B', '\x9C', '\x9D', '\x9E', '\x9F',
        '\xA0', '\xA1', '\xA2', '\xA3', '\xA4', '\xA5', '\xA6', '\xA7', '\xA8', '\xA9', '\xAA', '\xAB', '\xAC', '\xAD', '\xAE', '\xAF',
        '\xB0', '\xB1', '\xB2', '\xB3', '\xB4', '\xB5', '\xB6', '\xB7', '\xB8', '\xB9', '\xBA', '\xBB', '\xBC', '\xBD', '\xBE', '\xBF',
        '\xC0', '\xC1', '\xC2', '\xC3', '\xC4', '\xC5', '\xC6', '\xC7', '\xC8', '\xC9', '\xCA', '\xCB', '\xCC', '\xCD', '\xCE', '\xCF'
    };
    static const unsigned char untable[] = { 0,'1','2','3','4','5','6','7','8','9','0',0,'.','_',0,0,0,'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z' };
    int32_t i, j;
    uint32_t hashlo, divlo, result, mask;
    unsigned char hashhi, divhi;
    char ch;
    int32_t h;
    unsigned char *p;
    char buffer[15];

    if (!name || !out || outsz < 1)
        return 0;

    for (h = 0L, p = (unsigned char *)name; *p; p++)
        h = h * 43L + htable[*p];

    for (i = 0; i < 43; i++)
    {
        buffer[j = 14] = '\0';
        hashlo = (uint32_t)h;
        hashhi = (unsigned char)i;
        while (1)
        {
            divhi = 21;
            divlo = 0x80000000UL;
            result = 0UL;
            for (mask = 0x80000000UL; mask; mask >>= 1)
            {
                if (hashhi > divhi || (hashhi == divhi && hashlo >= divlo))
                {
                    result |= mask;
                    hashhi -= divhi;
                    if (divlo > hashlo) hashhi--;
                    hashlo -= divlo;
                }
                divlo >>= 1;
                if (divhi & 1) divlo |= 0x80000000UL;
                divhi >>= 1;
            }
            ch = (char)untable[(int32_t)hashlo];
            if (!ch) break;
            buffer[--j] = ch;
            if (result == 0)
            {
                strlcpy(out, buffer + j, outsz);
                return 1;
            }
            hashlo = result;
        }
    }
    sprintf(buffer, "HASH%08X", (unsigned int)h);
    strlcpy(out, buffer, outsz);
    return 1;
}

/*
 * Decompile a WinHelp/MVB file into its full RTF source ("mode 0"),
 * preserving context names (# footnotes), titles, keywords, jumps
 * ({\v ...}) and image references ({bmc ...}). Bitmaps are exported
 * next to the RTF. The generated .rtf/.hpj/.ico/.ph files land in the
 * directory of outBase.
 *
 * hlpPath  - path of the .hlp file to open (read only)
 * outBase  - output base name WITHOUT extension, e.g. "C:/tmp/helpreader_dx".
 *            The RTF file is written to outBase + ".rtf".
 *
 * Returns 1 on success, 0 on failure.
 */
int helpdeco_convert_to_rtf(const char *hlpPath, const char *outBase)
{
    char drive[_MAX_DRIVE];
    char dir[PATH_MAX];
    char filename[PATH_MAX];
    char work[_MAX_DRIVE + PATH_MAX];
    char cwd[PATH_MAX];
    FILE *f;

    if (!hlpPath || !outBase)
        return 0;

    init_hash_table();

    strlcpy(HelpFileName, hlpPath, sizeof(HelpFileName));
    _splitpath(hlpPath, drive, dir, name, ext);
    if (ext[0] == '\0') strcpy(ext, ".hlp");
    mvp = toupper((unsigned char)ext[1]) == 'M';

    /* redirect all generated output into the requested directory */
    strlcpy(name, outBase, sizeof(name));

    f = fopen(HelpFileName, "rb");
    if (!f)
        return 0;

    /* HelpDeCompile writes bitmaps into the current directory and all
     * other output files relative to the global "name" (outBase), so
     * change into the target directory for the duration of the call. */
    _splitpath(outBase, drive, dir, filename, NULL);
    _makepath(work, drive, dir, "", "");
    if (_getcwd(cwd, sizeof(cwd)) && work[0])
    {
        _chdir(work);
    }

    if (!HelpDeCompile(f, NULL, 0, NULL, 0))
    {
        _chdir(cwd);
        fclose(f);
        return 0;
    }

    _chdir(cwd);
    fclose(f);
    return 1;
}

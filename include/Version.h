#ifndef VERSION_H
#define VERSION_H

typedef struct
{
    int major;
    int minor;
    int patch;
    char prerelease[16];
} Version;

#endif // VERSION_H
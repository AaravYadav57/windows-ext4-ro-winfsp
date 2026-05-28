# windows-ext4-ro-winfsp
A driver written with the help of WinFsp to read ext4 fs in MBR/RO mode.
# ext4fsp - ext4 Driver For windows

this lets you open linux ext4 drives in windows explorer like a normal drive letter. it is read only tho so u cant break ur linux files by mistake.

made using WinFsp which is same thing used for sshfs-win and other fuse stuff i think.

---

## FEATURES

| thing                   | works?              |
| ----------------------- | ------------------- |
| ext2/ext3/ext4          | yes(dk about ext2/3 |
| big files               | yes                 |
| image files (.img/.iso) | yes                 |
| physical disks          | yes                 |
| GPT and MBR             | only mbr bro        |
| drive letters           | yes                 |
| mount folders too       | yes                 |
| symlinks                | mostly              |
| write support           | NO (read only only) |
| windows explorer        | no                  |

other stuff:

* extent trees
* inline data
* htree dirs
* timestamps
* cache thing
* huge files support
* auto drive letters

---

# REQUIREMENTS

## 1) WinFsp (required)

download it from
https://winfsp.dev/rel/

when installing make sure u install:

* core
* developer/sdk thing

default path should be:
`C:\Program Files (x86)\WinFsp`

if u dont install this it wont work at all

---

## 2) compiler

### visual studio (recommended probably)

download visual studio build tools 2022 and install c++ workload.

https://visualstudio.microsoft.com/downloads/

### OR mingw/msys2 (idk if it works)

install msys2 then run:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

### cmake too maybe

https://cmake.org/download/

---

# BUILDING

## easy way

```bat
build.bat
```

debug:

```bat
build.bat debug
```

32 bit:

```bat
build.bat x86
```

output should be:

```text
build\ext4fsp.exe
```

---

## cmake way

```bat
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## ninja build

```bat
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

---

## mingw

run this in msys2 shell:

```bat
build_mingw.bat
```

---

# RUNNING

admin rights needed for real disks.

image files usually work without admin.

syntax:

```text
ext4fsp [options] <source> <mount-point>
```

---

## mount img file

```bat
ext4fsp ubuntu.img *
```

or

```bat
ext4fsp ubuntu.img Z:
```

or

```bat
ext4fsp ubuntu.img C:\linuxstuff
```

`*` auto picks a drive letter.

---

## mount partition from disk image

```bat
ext4fsp disk.img -p 1 *
```

---

## mount real physical drive

```bat
ext4fsp \\.\PhysicalDrive1 --list-partitions
```

then:

```bat
ext4fsp \\.\PhysicalDrive1 -p 2 *
```

---

## list partitions

```bat
ext4fsp disk.img --list-partitions
```

example output:

```text
GPT disk detected.

Partition 1 EFI thing
Partition 2 linux filesystem
Partition 3 linux swap
```

---

## debug mode

```bat
ext4fsp -d ubuntu.img *
```

---

# unmounting

press ctrl+c in the console window.

or use this:

```bat
"C:\Program Files (x86)\WinFsp\bin\launchctl-x64.exe" stop ext4fsp
```

---

# windows explorer support

after mounting it should show in "this pc" like normal drive.

you can:

* browse files
* open pictures/text/etc
* copy files out
* use cmd stuff like dir/type

but writing doesnt work because its read only on purpose.

---

# HOW IT WORKS (sorta)

```text
windows explorer
    ↓
winfsp driver
    ↓
ext4fsp.exe
    ↓
ext4 parser code
    ↓
disk/image file
```

source files:

```text
src/ext4fsp.c
src/ext4fs.c
src/diskio.c
```

---

# KNOWN ISSUES

* encrypted ext4 doesnt work
* no journal recovery
* sparse files weird in windows
* compressed ext4 not supported
* some files maybe show empty sometimes
* large dirs can be kinda slow first time

---

# TROUBLESHOOTING

| problem                    | fix                      |
| -------------------------- | ------------------------ |
| FspFileSystemCreate failed | install winfsp maybe     |
| access denied              | run as admin             |
| ext4 magic not found       | wrong partition probably |
| drive not showing          | try Z: instead of *      |
| empty files                | weird inode bug maybe    |
| slow folders               | cache warming up         |

---

# LICENSE

MIT License (will add later bro).

WinFsp has its own license stuff too.

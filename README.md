# Virtual mass storage

A plugin to create a ram disk to mount at uma0.

With this, very simple and fast IO is possible.

By default vmass creates storage of size 16MiB.

It is useful as fast temporary storage as its contents are wiped at power off or reboot.

Content is retained for suspend

# Access Speed

```
exFAT -> FAT16(vmass) : 13000KB/s
FAT16 -> FAT16(vmass) : 15000KB/s
```

# Installing

Add under \*KERNEL in Taihen config.txt

If vmass detects that uma0: is already mounted by another plugin, vmass will exit without creating virtual storage.

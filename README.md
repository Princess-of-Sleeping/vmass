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

# Saving vmass storage

Power off or reboot while holding the start button to save vmass storage.

Path where img is saved
```
sd0:vmass.img

If it cannot be saved to sd0, it will be saved to ux0.
ux0:data/vmass.img
```

If img was saved in these paths when vmass started, read them and restore the previous storage.

# Note
When a game, app, etc. is started in +109MB mode, it may operate incorrectly due to a lack of memory

# Installing

Add under \*KERNEL in Taihen config.txt

If vmass detects that uma0: is already mounted by another plugin, vmass will exit without creating virtual storage.

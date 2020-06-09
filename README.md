# Virtual mass storage

A plugin to create a ram disk to mount at uma0.

With this, very simple and fast IO is possible.

By default vmass creates storage of size 32MiB.

It is useful as fast temporary storage as its contents are wiped at power off or reboot.

Content is retained for suspend

# Installing

Add under \*KERNEL in Taihen config.txt

If you are using other plugins that may mount uma0, please disable them as they will conflict. Priority is given to the vmass plugin.

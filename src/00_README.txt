THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK

"SleepyPiPlayer" is a MP3-player with an automatic sleep-timer.
When using this software, use "mp3splt" and split your audiobook
into MP3-files of 5 minutes duration each.
You can use it in complete darkness without your glasses.

license: free software   (sleepypiplayer(at)saftfresse.de)  [Anno.2025]
 - Do whatever you want with the software.
 - THIS SOFTWARE COMES WITH NO WARRANTIES, USE AT YOUR OWN RISK

 -------------------------------------------------------
use-case
- press red button to switch on the mp3-player
- press the white file-back button some times
  (you will not remember all the playback, because you fell asleep)
- listen to the audio and fall asleep
- the player will automatically shutdown after some minutes

startup:  "File 17 of 59"
shutdown: "a keypress prevents shutdown  9 8 7 6 5 4 3 2 1 0"

If you do not hear the startup-text: the system-MP3-files are not found.
Check SYSTEM_SOUND_PATH in sleepy.cfg.

You can test the software without a raspberry:  compile_pc   ./try_pc

-------------------------------------------------------
features:
+ tactile buttons
+ always active automatic shutdown after some minutes
+ remembers the playback position
- raspberry boot-time is about 20 seconds

my hardware
- raspberry pi zero 2W
- Intenso 7313530 Powerbank XS 10000
- Adafruit Panel Mount Extension USB Cable
- Pimoroni OnOff SHIM (https://github.com/pimoroni/clean-shutdown)
- UGREEN External USB Sound Card Jack USB Adapter
- 24mm Arcade Buttons
- Adafruit Speaker - 3" Diameter - 4 Ohm 3 Watt
- sourcing map Speaker Grille Cover 3 Inch 95 mm Mesh

helpful
hole in the housing through which you can see the powerbank-leds

my source of hardware recommendations
https://menner-maidi.de/phoniebox-diy-alternative-zur-toniebox/

-------------------------------------------------------
MP3-files: audio-book split to 5-minute files

lame -h --noreplaygain --cbr -b 64 -s 32 -m m original.mp3 small_size.mp3
mp3gain -r small_size.mp3
mp3splt -f -t 5.0 -a -o @f_@m4 -g "r%[@t=MyTitle_@m]" small_size.mp3

-------------------------------------------------------
compile on linux-PC for easier development and testing
- cd sleepy
- check sleepy.cfg file  DISABLE_WIFI=OFF  AUTO_SHUTDOWN=-1
- chmod 447 compile_pc
- ./compile_pc
- check sleepy.cfg again, just to be sure
- ./try_pc

-------------------------------------------------------
keys
- (GPIO_17-red)    on/off
- (GPIO_20-black)  quiet single-press:  change audio-volume  20..100%
- (GPIO_19-black)  loud  single-press:  change audio-volume  20..100%
- (GPIO_21-white)  back  single-press:  one file back
- (GPIO_21-white)  back  long-press:    playback fast backward
- (GPIO_13-white)  forw. single-press:  one file forward
- (GPIO_13-white)  forw. long-press:    playback fast forward
- (GPIO_16-yellow) info  single-press:  information about playback directory file position
- (yellow-long-press and white-single-press):  change directory
- (yellow-long-press and white-long-press):    change files forward/backward (fast)
- (GPIO_26-blue-long-press and both white-long-press): service-mode

-------------------------------------------------------

service-mode - replace MP3-files on raspberry
 raspberry stops playing and turns on wifi
 login to raspberry
   ssh dietpi@192.168.XX.XX
 enable read-write for mp3
   mount -o remount,rw /SLEEPY_MP3
 Service-mode will shutdown immediately if red-button is pressed.
 After 20 minutes, service-mode will start shutdown in 15 minutes.
 If you want to change the firmware:
   sudo systemctl stop sleepy
   sudo shutdown -c
   mount -o remount,rw /SLEEPY_FW
   [do whatever you want]
   sudo reboot
 If you want to change something on the root-file-system:
   mount -o remount,rw /
 If you want to inspect changes on the SD-card
   umount /dev/mmcblk0p3
   cat /dev/mmcblk0p3 | ssh user@192.168.XX.XX 'cat > /home/XX/raspi_fw.bin'
   (use hexdump and kdiff3 to search for differences)



=======================================================
=======================================================
=======================================================
 STEP BY STEP instructions to prepare SD-card
 (working 2025-12-30)
=======================================================
=======================================================

prepare SD-card on a LINUX-PC

Use DietPi:
It boots 5 seconds faster than regular Raspberry Pi OS.

Download image from https://dietpi.com
Raspberry Pi 2/3/4/Zero 2   BCM2710/2711 900-1800 MHz | 4 Cores | ARMv8
xz -d DietPi_RPi234-ARMv8-Trixie.img.xz
sd-card  linux-mint:   sudo mintstick -m iso

create some additional partitions on your linux-PC by using "GParted".
-- see misc/gparted.png
- shutdown your PC
- remove the SD-card
- reboot   your PC before using GParted
- insert the SD-card
- lsblk  # check device-name
- umount SD-card  e.g. "umount /dev/sdc1" / "umount /dev/sdc2"
- start gparted as root
- select your SD-card in gparted (do not edit your hard-drive)
- change name of partition.1 to "SLEEPY_BOOT"  # e.g. /dev/sdc1  context-menu "Label File System"
- change name of partition.2 to "SLEEPY_root"  # e.g. /dev/sdc2  context-menu "Label File System"
- change size of partition.2 "SLEEPY_root" to 2944 MiB  # e.g. /dev/sdc2  context-menu "resize"
- create partition.3 with size 256 MiB (primary / ext.4)        "SLEEPY_FW"   # "unallocated"-context-menu: "new"
- create partition.4 "extended partition" remaining disk size                 # "unallocated"-context-menu: "new"
- create partition.5 with size 512 MiB (logic / fat32)          "SLEEPY_SAVE" # "unallocated"-context-menu: "new"
- create partition.6 with remaining disk size (logic / ext.4)   "SLEEPY_MP3"  # "unallocated"-context-menu: "new"
-> write the changes to disk (toolbar - arrow/check-symbol)
-> close gparted

(Partition 1 and 2 are already created by the dietpi-image.)
Partition.1 Fat32  128M  created by DietPi image         /boot
partition.2 ext4  2944M  created by DietPi image         /
partition.3 ext4   256M  sleepy   (software)             /SLEEPY_FW
partition.4 extended partition remaining disk size
partition.5 Fat32  512M  save/storage.bin                /SLEEPY_SAVE
partition.6 ext4  ????M  mp3                             /SLEEPY_MP3

mount sdc1=(SLEEPY_BOOT)  and  sdc2=(SLEEPY_root)

sudo mkdir /media/xxx/SLEEPY_root/SLEEPY_FW
sudo mkdir /media/xxx/SLEEPY_root/SLEEPY_SAVE
sudo mkdir /media/xxx/SLEEPY_root/SLEEPY_MP3

Edit some settings on the SD-Card

sudo nano /media/xxx/SLEEPY_root/etc/fstab
  change size of /tmp to 7M
    tmpfs /tmp tmpfs size=7M,noatime,lazytime,nodev,nosuid,mode=1777
  change size of /var/log to 5M
    tmpfs /var/log tmpfs size=5M,noatime,lazytime,nodev,nosuid
  add new partitions  (replace xxxxxxxx: see already existing partition.1)
    PARTUUID=xxxxxxxx-03 /SLEEPY_FW      ext4    defaults,noatime,lazytime,rw              0    2
    PARTUUID=xxxxxxxx-05 /SLEEPY_SAVE    vfat    defaults,noatime,dmask=000,fmask=111      0    2
    PARTUUID=xxxxxxxx-06 /SLEEPY_MP3     ext4    defaults,noatime,lazytime,rw              0    2

nano /media/xxx/SLEEPY_BOOT/dietpi-wifi.txt
  aWIFI_SSID[0]='my_wifi'
  aWIFI_KEY[0]='my_passwd'
  aWIFI_KEYMGR[0]='WPA-PSK'

nano /media/xxx/SLEEPY_BOOT/dietpi.txt
  AUTO_SETUP_KEYBOARD_LAYOUT=?? e.g. "de"
  AUTO_SETUP_NET_ETHERNET_ENABLED=0
  AUTO_SETUP_NET_WIFI_ENABLED=1
  AUTO_SETUP_NET_WIFI_COUNTRY_CODE=?? e.g. "DE"
  AUTO_SETUP_BOOT_WAIT_FOR_NETWORK=0
  AUTO_SETUP_SWAPFILE_SIZE=0
  CONFIG_SERIAL_CONSOLE_ENABLE=0
  AUTO_SETUP_RAMLOG_MAXSIZE=5
  SURVEY_OPTED_IN=0


*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*
*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*

insert the SD-card into the raspberry and login via SSH
(get the ip-address from your WIFI-router DHCP-server)
(password: "dietpi")
ssh root@192.168.1.123

Think of a dietpi root-password and write it down on the inside of the device lid.
The password does not need to be that secure, 
because the network-connection is normally switched off.

-----

# by the way: central config tool: dietpi-launcher
you are now in dietpi-sortware   ESC=back
(see misc/dietpi-software.png)
  survey: no  ("opt out")
  password: set the same password to all entities
  UART: disable <yes>
  select "DietPi-Config"
    * "Display Options"
        "Display Resolution"
           -> set "Headless"
        "LED Control"
          "ACT"
            -> set "none"
          "default-on"
            -> set "none"
          "mmc0"
            -> set "none"
    * "Audio Options"
        "Enable: install ALSA"
          -> OK
        "Sound card"
          -> set "hw:0,0" : Device USB Audio
        "Auto-conversion"
          -> set "On"
    * "Advanced Options"
        "Swap file"
          -> set "Off"
        "Time sync mode"
          -> set "Custom"
  -> "SSH-server"  change to "OpenSSH Server"
  -> "Log System"  keep      "DeitPi-RAMlog#1"
  -> "Install"     select "OK"

-----

create the directories on raspberry

sudo mkdir     /SLEEPY_FW/sleepy
sudo chmod 777 /SLEEPY_FW/sleepy

sudo mkdir     /SLEEPY_MP3/mp3
sudo chmod 777 /SLEEPY_MP3/mp3

sudo mkdir /SLEEPY_SAVE/save

-----

add user dietpi to group gpio: allow setting input-output-pins

sudo usermod -aG gpio dietpi

-----

add user dietpi to group audio: allow access to alsa-sound

sudo usermod -aG audio dietpi

-----

sudo reboot

-----

ssh dietpi@192.168.1.123
  sudo apt-get update
  sudo apt-get upgrade
  sudo reboot

-----

remove swap-file: we try to avoid writing to the SD-card
(It was added by the dietpi-software in the meanwhile.)

ssh dietpi@192.168.1.123
  sudo nano /etc/fstab
  -> remove this line:  /var/swap none swap sw
  sudo reboot

ssh dietpi@192.168.1.123
   sudo rm  /var/swap

------

copy files from linux-PC  to raspberry

scp -r SysMP3     dietpi@192.168.1.123:/SLEEPY_FW/sleepy
scp -r src        dietpi@192.168.1.123:/SLEEPY_FW/sleepy
scp -r misc       dietpi@192.168.1.123:/SLEEPY_FW/sleepy
scp -r audiobook1 dietpi@192.168.1.123:/SLEEPY_MP3/mp3
scp -r audiobook2 dietpi@192.168.1.123:/SLEEPY_MP3/mp3
scp -r audiobook3 dietpi@192.168.1.123:/SLEEPY_MP3/mp3

-------------------------------------------------------

Make sure a shutdown will turn off power.

Using original ON-OFF-SHIM software causes trouble.
(https://github.com/pimoroni/clean-shutdown)
Instead of original-package better just copy this script:

ssh root@192.168.1.123
   sudo mkdir /usr/lib/systemd/system-shutdown
   sudo cp  /SLEEPY_FW/sleepy/misc/onoff-shim /usr/lib/systemd/system-shutdown
   sudo chmod 755 /usr/lib/systemd/system-shutdown/onoff-shim

-------------------------------------------------------

set speaker-volume to about 85 percent
set microphone to 0 percent
   sudo alsamixer

-----

install some packages
  sudo apt-get --yes install libout123-0
  sudo apt-get --yes install libmpg123-dev
  sudo apt-get --yes install mpg123
  sudo apt-get --yes install libgpiod-dev
  sudo apt-get --yes install dbus
  sudo apt-get --yes install rfkill
  sudo apt-get --yes install less
  sudo apt-get --yes install g++

sudo systemctl unmask systemd-logind
sudo systemctl start dbus systemd-logind

-----

compile the sourcecode on raspberry
  cd /SLEEPY_FW/sleepy/src
  chmod 777 compile_raspi_gpiod_1_6
  ./compile_raspi_gpiod_1_6

if executing compile_raspi_gpiod_1_6 causes errors: try gpiod_2_1
  apt list --installed | grep gpiod
  cd /SLEEPY_FW/sleepy/src
  chmod 777 compile_raspi_gpiod_2_1
  ./compile_raspi_gpiod_2_1

/SLEEPY_FW/sleepy$  ln -s src/sleepy     sleepy
/SLEEPY_FW/sleepy$  ln -s src/sleepy.cfg sleepy.cfg

-----

configure sleepy
DISABLE_WIFI should be OFF  as long as the system is not fully operational
  nano /SLEEPY_FW/sleepy/src/sleepy.cfg

-------------------------------------------------------

automatic startup of sleepy-firmware
sudo cp /SLEEPY_FW/sleepy/src/sleepy.service /etc/systemd/system
sudo systemctl enable sleepy

check sleepy.cfg
DISABLE_WIFI should be OFF  as long as the system is not fully operational

reboot and make sure sleepy is executed and plays audio
problems? you might have to edit /SLEEPY_FW/sleepy/src/sleepy.cfg
sudo reboot

If the raspi plays audio, press the onoff-button
-> shutdown - poweroff should work now
   (but writing the playback-position for the first time will take some time)

*--*--*--* --*--*--*--*--*--*--*--*--*--*--*--*--*--*--*

check the boot-duration before changing to read-only-filesystem
   systemd-analyze critical-chain
   systemd-analyze plot > boot_time.svg
   systemctl list-unit-files
   systemctl list-units
   systemctl list-dependencies networking
trace file-modifications
   inotifywait --monitor --event modify -r /tmp /var /run /sbin /etc /srv /usr

perform boot-time optimization by turning off FileSystem-check
(we will change to read-only file-system later)

sudo nano /boot/cmdline.txt
    replace "fsck.repair=yes" with "fsck.mode=skip"

sudo nano /etc/fstab
  disable FileSystemCheck: change last digit of each line to "0"    (startup is 0.2s faster)
   PARTUUID=xxxxxxxx-01  /boot         vfat    noatime,lazytime,ro                      0    2
   PARTUUID=xxxxxxxx-02  /             ext4    noatime,lazytime,ro                      0    1
   PARTUUID=xxxxxxxx-03 /SLEEPY_FW     ext4    defaults,noatime,lazytime,ro             0    2
   PARTUUID=xxxxxxxx-05 /SLEEPY_SAVE   vfat    defaults,noatime,dmask=000,fmask=111     0    2
   PARTUUID=xxxxxxxx-06 /SLEEPY_MP3    ext4    defaults,noatime,lazytime,ro             0    2
  ----
   PARTUUID=xxxxxxxx-01  /boot         vfat    noatime,lazytime,ro                      0    0
   PARTUUID=xxxxxxxx-02  /             ext4    noatime,lazytime,ro                      0    0
   PARTUUID=xxxxxxxx-03 /SLEEPY_FW     ext4    defaults,noatime,lazytime,ro             0    0
   PARTUUID=xxxxxxxx-05 /SLEEPY_SAVE   vfat    defaults,noatime,dmask=000,fmask=111     0    0
   PARTUUID=xxxxxxxx-06 /SLEEPY_MP3    ext4    defaults,noatime,lazytime,ro             0    0


Disable WIFI in normal operation: save power

nano /SLEEPY_FW/sleepy/src/sleepy.cfg
  DISABLE_WIFI ON

-> from now on you need to start service-mode in order to use SSH

---

restart by using red onoff-button
WIFI is off: you need the service-mode to use ssh


*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*

avoid writing to the SD-card
ATTENTION:  only try this if your system is fully operational
If something goes wrong, you could be forced clear the SD-card
and repeat all installation-steps so far.

SD-cards only allow a limited amount of write-cycles.
In normal conditions, writing the playback-position
is the only required write-operation.

Even if a partition is mounted/unmounted read-only,
a changed mount/unmount-counter is written to the SD-card.

If a partition is mounted/unmounted read-write without writing a file,
additional changes are written to the SD-card.

ADVICE: use industrial micro-SD-cards with SLC-Flash

sudo nano /etc/fstab
# /etc/fstab  --  mount root-file-system read-only
# add option "ro" : read-only   to partition.1,  partition.2, partition.3 and partition.6
# => keep partition.5 "SLEEPY_SAVE" read-write
# problems with WIFI: you can still remove the SD-card and edit fstab and all other files.
PARTUUID=xxxxxxxx-01  /boot         vfat    noatime,lazytime,ro                      0    2
PARTUUID=xxxxxxxx-02  /             ext4    noatime,lazytime,ro                      0    1
PARTUUID=xxxxxxxx-03 /SLEEPY_FW     ext4    defaults,noatime,lazytime,ro             0    2
PARTUUID=xxxxxxxx-05 /SLEEPY_SAVE   vfat    defaults,noatime,dmask=000,fmask=111     0    2
PARTUUID=xxxxxxxx-06 /SLEEPY_MP3    ext4    defaults,noatime,lazytime,ro             0    2

---

check the size of tmpfs again
sudo nano /media/xxx/root/etc/fstab
  change size of /tmp to 7M
    tmpfs /tmp tmpfs size=7M,noatime,lazytime,nodev,nosuid,mode=1777
  change size of /var/log to 5M
    tmpfs /var/log tmpfs size=5M,noatime,lazytime,nodev,nosuid

---

sudo reboot

---

# if you need to write to the root-file-system after ssh-login
sudo mount -o remount,rw /
sudo mount -o remount,rw /SLEEPY_MP3
sudo mount -o remount,rw /SLEEPY_FW
partition 5 remains read-write   "SLEEPY_SAVE"

check  /SLEEPY_FW/sleepy/src/sleepy.cfg
DISABLE_WIFI ON

----------------------------------------------------------

After copying the mp3-files to /SLEEPY_mp3/mp3
you could try to optimize the directory-index

# device is busy
ps -aux | grep sleepy
sudo kill 321

use lsblk to get the device-name

sudo umount /SLEEPY_MP3
sudo tune2fs -O dir_index /dev/mmcblk0p6
sudo e2fsck -fD /dev/mmcblk0p6
sudo reboot

----------------------------------------------------------

sudo poweroff


=======================================================
=======================================================
=======================================================
Update MP3-Files on raspberry
=======================================================
=======================================================
- turn on the raspberry-player
- enter Service-Mode by long-press of blue + white + white
- ssh dietpi@192.168.X.XXX
  (Get the ip-address from your WIFI-router DHCP-server.)
  (The password could be on the inside of the device lid.)
  - sudo mount -o remount,rw /SLEEPY_MP3
  - rm /SLEEPY_MP3/mp3/audiobook2
- copy new audiobook from PC to raspberry
  - scp -r audiobook4 dietpi@192.168.X.XXX:/SLEEPY_MP3/mp3
- leave Service-Mode by pressing onoff-button (red)

Attention:
Starting a WIFI-connection requires write-access to /etc/resolv.conf
Because of this, the service-mode remounts the root-file-system  read-write.
("sudo mount -o remount,rw /")


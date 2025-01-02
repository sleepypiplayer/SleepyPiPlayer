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
 (working 2024-12-20)
=======================================================
=======================================================

Use DietPi:
It boots 5 seconds faster than regular Raspberry Pi OS.

Download image from https://dietpi.com
Raspberry Pi 2/3/4/Zero 2   BCM2710/2711 900-1800 MHz | 4 Cores | ARMv8
xz -d DietPi_RPi-ARMv8-Bookworm.img.xz
sd-card  linux-mint:   mintstick -m iso

lsblk
sudo umount /dev/sdd1
sudo umount /dev/sdd2
sudo fatlabel /dev/sdd1 BOOT
sudo e2label  /dev/sdd2 root

mount sdd1=(BOOT)  and  sdd2=(ROOT)

sudo mkdir /media/xxx/root/SLEEPY_FW
sudo mkdir /media/xxx/root/SLEEPY_SAVE
sudo mkdir /media/xxx/root/SLEEPY_MP3

Edit some settings on the SD-Card

nano /media/xxx/BOOT/dietpi.txt
  AUTO_SETUP_KEYBOARD_LAYOUT=?? e.g. "de"
  AUTO_SETUP_NET_ETHERNET_ENABLED=0
  AUTO_SETUP_NET_WIFI_ENABLED=1
  AUTO_SETUP_NET_WIFI_COUNTRY_CODE=?? e.g. "DE"

nano /media/xxx/BOOT/dietpi-wifi.txt
  aWIFI_SSID[0]='my_wifi'
  aWIFI_KEY[0]='my_passwd'
  aWIFI_KEYMGR[0]='WPA-PSK'

sudo nano /media/xxx/root/etc/fstab
  change size of /tmp to 7M
    tmpfs /tmp tmpfs size=7M,noatime,lazytime,nodev,nosuid,mode=1777
  change size of /var to 5M
    tmpfs /var/log tmpfs size=5M,noatime,lazytime,nodev,nosuid
  add new partitions  (replace xxxxxxxx: see partition.1)
    PARTUUID=xxxxxxxx-03 /SLEEPY_FW      ext4    defaults,noatime,lazytime                 0    2
    PARTUUID=xxxxxxxx-05 /SLEEPY_SAVE    vfat    defaults,noatime,dmask=000,fmask=111      0    2
    PARTUUID=xxxxxxxx-06 /SLEEPY_MP3     ext4    defaults,noatime,lazytime                 0    2


-------------------------------------------------------

Before inserting the SD-card into the raspberry,
create some additional partitions by using "GParted".
- shutdown your PC after writing the OperatingSystem
- remove the SD-card
- reboot   your PC before using GParted
- insert the SD-card
- umount SD-card   "umount /dev/sde1" / "umount /dev/sde2"
- start gparted as root
- select your SD-card in gparted (do not edit your hard-drive)
- change size of partition.2 "rootfs" to 2560 MiB
- create partition.3 with size 512 MiB (primary / ext.4)        "SLEEPY_FW"
- create partition.4 "extended partition" remaining disk size
- create partition.5 with size 512 MiB (logic / fat32)          "SLEEPY_SAVE"
- create partition.6 with remaining disk size (logic / ext.4)   "SLEEPY_MP3"
-> write the changes to disk (toolbar - arrow-symbol)
-> close gparted

(Partition 1 and 2 are already created by the dietpi-image.)
Partition.1 Fat32  128M  created by DietPi image         /boot
partition.2 ext4  2560M  created by DietPi image         /
partition.3 ext4   512M  sleepy   (software)             /SLEEPY_FW
partition.4 extended partition remaining disk size
partition.5 Fat32  512M  save/storage.bin                /SLEEPY_SAVE
partition.6 ext4  ????M  mp3                             /SLEEPY_MP3

-----

create the directories on linux-PC or on raspberry

mkdir     /SLEEPY_FW/sleepy
chmod 777 /SLEEPY_FW/sleepy

mkdir     /SLEEPY_MP3/mp3
chmod 777 /SLEEPY_MP3/mp3

mkdir /SLEEPY_SAVE/save

copy files to /SLEEPY_FW/sleepy/SysMP3
copy files to /SLEEPY_FW/sleepy/Documentation
copy files to /SLEEPY_FW/sleepy/src
copy files to /SLEEPY_MP3/mp3/audiobook1
copy files to /SLEEPY_MP3/mp3/audiobook2
copy files to /SLEEPY_MP3/mp3/audiobook3

SLEEPY_FW/sleepy$  ln -s /src/sleepy     sleepy
SLEEPY_FW/sleepy$  ln -s /src/sleepy.cfg sleepy.cfg


*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*

insert the SD-card into the raspberry and login via SSH
(get the ip-address from your WIFI-router DHCP-server)
(password: "dietpi")
ssh root@192.168.1.123

-----

# by the way: central config tool: dietpi-launcher
you are now in dietpi-sortware   ESC=back
  survey: no  ("opt out")
  password: set the same password to all entities
  UART: disable <yes>
  select "DietPi-Config"
    "Display Options"
      "Display Resolution"
         -> set "Headless"
      "LED Control"
        "ACT"
          -> set "none"
        "PWR"
          -> set "none"
    "Audio Options"
      "Enable: install ALSA"
        -> OK
      "Sound card"
        -> set "hw:0,0" : Device USB Audio
      "Auto-conversion"
        -> set "On"
  -> "SSH-server"  change to "OpenSSH Server"
  -> "Log System"  keep      "DeitPi-RAMlog#1"
  -> "Install"     select "OK"

sudo reboot

-----

ssh dietpi@192.168.1.123
  sudo apt-get update
  sudo apt-get upgrade
  sudo reboot

-------------------------------------------------------

sudo nano /etc/fstab
  change size of /tmp to 7M
    tmpfs /tmp tmpfs size=7M,noatime,lazytime,nodev,nosuid,mode=1777
  change size of /var/log to 5MB
    tmpfs /var/log tmpfs size=5M,noatime,lazytime,nodev,nosuid

-------------------------------------------------------

install  ON-OFF-SHIM  software

ssh dietpi@192.168.1.123
sudo curl https://get.pimoroni.com/onoffshim | bash
-> reboot ->  "no"


### problems with raspbery key-press-detection ??? ###
  https://github.com/raspberrypi/linux/issues/6037
    /sys/class/gpio  numbers have changed by 512

Also OnOff-shim is affected:
    https://github.com/pimoroni/clean-shutdown
    https://github.com/pimoroni/clean-shutdown/issues/37

Replace /sys/class/gpio/... with pinctrl
  sudo nano /lib/systemd/system-shutdown/gpio-poweroff
        #/bin/echo $poweroff_pin > /sys/class/gpio/export
        #/bin/echo out > /sys/class/gpio/gpio$poweroff_pin/direction
        #/bin/echo 0 > /sys/class/gpio/gpio$poweroff_pin/value
        pinctrl set $poweroff_pin ip pd
        /bin/sleep 0.5

-------------------------------------------------------

set speaker-volume to about 85 percent
set microphone to 0 percent
   sudo alsamixer

-----

install some packages
  sudo apt-get install libout123-0
  sudo apt-get install libmpg123-dev
  sudo apt-get install mpg123
  sudo apt-get install libgpiod-dev
  sudo apt-get install dbus
  sudo apt-get install rfkill
  sudo apt-get install g++
  sudo apt-get install less

sudo systemctl unmask systemd-logind
sudo systemctl start dbus systemd-logind

-----

compile the sourcecode on raspberry
  cd /SLEEPY_FW/sleepy/src
  chmod 447 compile_raspi_gpiod_1_6
  ./compile_raspi_gpiod_1_6

-----

configure sleepy
DISABLE_WIFI should be OFF  as long as the system is not fully operational
  nano /SLEEPY_FW/sleepy/src/sleepy.cfg

-------------------------------------------------------

automatic startup of sleepy-firmware
sudo cp sleepy.service /etc/systemd/system
sudo systemctl enable sleepy

check sleepy.cfg
DISABLE_WIFI should be OFF  as long as the system is not fully operational

reboot and make sure sleepy is executed and plays audio
problems? you might have to edit /SLEEPY_FW/sleepy/src/sleepy.cfg
sudo reboot


*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*--*

check the boot-duration before changing to read-only-filesystem
   systemd-analyze critical-chain
   systemd-analyze plot > boot_time.svg
   systemctl list-unit-files
   systemctl list-units
   systemctl list-dependencies networking


perform some boot-time optimizations by turning off network-services
ATTENTION:  only try this if your system is fully operational
If something goes wrong, you could be forced clear the SD-card
and repeat all installation-steps so far.

sudo systemctl disable ifup@wlan0
sudo systemctl disable wpa_supplicant
sudo systemctl disable networking
sudo systemctl disable NetworkManager
sudo systemctl disable bluetooth
sudo systemctl disable systemd-rfkill
sudo systemctl disable systemd-random-seed
sudo systemctl disable systemd-timesyncd

nano /SLEEPY_FW/sleepy/src/sleepy.cfg
  DISABLE_WIFI ON

-> from now on you need to start service-mode in order to use SSH

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

- disable filesystem-check: we will become read-only
  sudo systemctl disable systemd-fsckd


sudo nano /etc/fstab
# /etc/fstab  --  mount root-file-system read-only
# add option "ro" : read-only   to partition.1,  partition.2, partition.3 and partition.6
# => keep partition.5 "SLEEPY_SAVE" read-write
# problems with WIFI: you can still remove the SD-card and edit fstab and all other files.
PARTUUID=59786d62-01  /boot         vfat    noatime,lazytime,ro                      0    2
PARTUUID=59786d62-02  /             ext4    noatime,lazytime,ro                      0    1
PARTUUID=59786d62-03 /SLEEPY_FW     ext4    defaults,noatime,lazytime,ro             0    2
PARTUUID=59786d62-05 /SLEEPY_SAVE   vfat    defaults,noatime,dmask=000,fmask=111     0    2
PARTUUID=59786d62-06 /SLEEPY_MP3    ext4    defaults,noatime,lazytime,ro             0    2

---

check
sudo nano /media/xxx/root/etc/fstab
  change size of /tmp to 7M
    tmpfs /tmp tmpfs size=7M,noatime,lazytime,nodev,nosuid,mode=1777
  change size of /var/log to 5M
    tmpfs /var/log tmpfs size=5M,noatime,lazytime,nodev,nosuid

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
